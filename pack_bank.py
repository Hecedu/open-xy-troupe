"""Lossless full-wave Rice blocks with optional bounded Q14 predictors.

Every predictor error is stored exactly. Coefficient quantization changes the
residual representation, never the decoded waveform. Finite differences are
retained whenever smaller. The audio-rate oscillators never run this decoder.
"""
from pathlib import Path
import hashlib
import json
import numpy as np

FOLDER = Path(__file__).resolve().parent
BLOCK = 256
SEED_BITS = 14
OFFSET_BITS = 22
OFFSET_MASK = (1 << OFFSET_BITS) - 1


def authored_bank():
    bank = np.array([np.fromstring(line.strip().strip('{},'), sep=',', dtype=np.int32)
                     for part in range(4) for line in (FOLDER / f'bank_{part}.h').read_text().splitlines()])
    if bank.shape != (1450, 256) or bank.min() < -8192 or bank.max() > 8191:
        raise ValueError('Unexpected bank dimensions or exact14-bit seed range')
    return bank


def predictor_source(bank):
    data = json.loads((FOLDER / 'predictors.json').read_text())
    if (data['schema'], data['shift'], data['coefficient_bits'], data['absolute_sum_limit']) != (1, 14, 18, 15 * 16384):
        raise ValueError('Unexpected predictor format')
    if data['bank_sha256'] != hashlib.sha256(bank.astype('<i2').tobytes()).hexdigest() or len(data['predictors']) != 1450:
        raise ValueError('Predictors do not belong to this exact authored bank')
    for row in data['predictors']:
        if set(row) - {'2', '3', '4'}:
            raise ValueError('Unexpected predictor order')
        for order, coefficients in row.items():
            if (len(coefficients) != int(order) or any(type(value) is not int or not -131072 <= value < 131072 for value in coefficients)
                    or sum(abs(value) for value in coefficients) > 15 * 16384):
                raise ValueError('Predictor exceeds the signed32-bit arithmetic bound')
    return data['predictors']


def rice_choice(residual, header_bits, linear, order):
    zigzag = np.where(residual < 0, -residual * 2 - 1, residual * 2)
    choices = []
    for k in range(20):
        quotient = zigzag >> k
        if quotient.max() <= 15 and (quotient + 1 + k).max() <= 32:
            choices.append((header_bits + int((quotient + 1 + k).sum()), linear, order, k, zigzag))
    return min(choices, key=lambda entry: entry[:4])


def choose(block, predictors):
    choices = [rice_choice(np.diff(block.astype(np.int64), n=order), order * SEED_BITS, 0, order)
               for order in range(1, 5)]
    for text_order, coefficients in predictors.items():
        order = int(text_order)
        history = np.stack([block[order - index - 1:255 - index] for index in range(order)], axis=1).astype(np.int64)
        dot = (history * np.array(coefficients, dtype=np.int64)).sum(axis=1)
        if np.max(np.abs(dot)) > 2147483647:
            raise RuntimeError('Predictor multiplication/sum bound violated')
        prediction = np.where(dot >= 0, dot // 16384, -((-dot) // 16384))
        choices.append(rice_choice(block[order:].astype(np.int64) - prediction,
                                   order * (SEED_BITS + 18), 1, order))
    return min(choices, key=lambda entry: entry[:4])


def pack(bank):
    predictors = predictor_source(bank)
    descriptors, payloads, cursor, blob = [], {}, 0, 0
    stats = dict(rice_blocks=0, linear_blocks=0, difference_blocks=0, orders=[0, 0, 0, 0], max_quotient=0, max_code_bits=0)
    for table, block in enumerate(bank):
        length, linear, order, parameter, residuals = choose(block, predictors[table])
        bits, payload = 0, 0
        for sample in block[:order]:
            payload |= (int(sample) & 16383) << bits
            bits += SEED_BITS
        if linear:
            for coefficient in predictors[table][str(order)]:
                payload |= (coefficient & ((1 << 18) - 1)) << bits
                bits += 18
        mask = (1 << parameter) - 1
        for residual in residuals:
            value = int(residual)
            quotient = value >> parameter
            payload |= ((1 << quotient) | ((value & mask) << (quotient + 1))) << bits
            code_bits = quotient + 1 + parameter
            bits += code_bits
            stats['max_quotient'] = max(stats['max_quotient'], quotient)
            stats['max_code_bits'] = max(stats['max_code_bits'], code_bits)
        if bits != length:
            raise RuntimeError('Incorrect packed length')
        key = (bits, payload)
        if key in payloads:
            offset = payloads[key]
        else:
            offset = cursor
            payloads[key] = offset
            blob |= payload << cursor
            cursor += bits
        if cursor >= 1 << OFFSET_BITS:
            raise ValueError('Compact offset range exceeded')
        descriptors.append(offset | (parameter << 22) | ((order - 1) << 27) | (linear << 29))
        stats['rice_blocks'] += 1
        stats['linear_blocks' if linear else 'difference_blocks'] += 1
        stats['orders'][order - 1] += 1
    words = [(blob >> bit) & 0xffffffff for bit in range(0, cursor, 32)] + [0, 0]
    stats['payload_bits'] = cursor
    stats['unique_payloads'] = len(payloads)
    return descriptors, words, stats


def window(words, bit):
    return ((words[bit // 32] | (words[bit // 32 + 1] << 32)) >> (bit & 31)) & 0xffffffff


def signed(value, width):
    value &= (1 << width) - 1
    sign = 1 << (width - 1)
    return (value & (sign - 1)) - (value & sign)


def decode_slice(descriptors, words, table, start, count):
    if not 0 <= table < 1450 or not 0 < count <= BLOCK or not 0 <= start < BLOCK or start + count > BLOCK:
        raise ValueError('Slice crosses a fixed block')
    descriptor = descriptors[table]
    bit = descriptor & OFFSET_MASK
    parameter = descriptor >> 22 & 31
    order = (descriptor >> 27 & 3) + 1
    linear = bool(descriptor & (1 << 29))
    seeds, coefficients = [], []
    for _ in range(order):
        seeds.append(signed(window(words, bit), SEED_BITS));bit += SEED_BITS
    if linear:
        for _ in range(order):
            coefficients.append(signed(window(words, bit), 18));bit += 18
    differences = [int(np.diff(seeds, n=degree)[-1]) for degree in range(order)]
    values = seeds.copy()
    for position in range(order, start + count):
        view = window(words, bit)
        quotient = 0
        while not view & (1 << quotient):
            quotient += 1
            if quotient > 15:
                raise RuntimeError('Rice prefix exceeds its fixed bound')
        unsigned = (quotient << parameter) | ((view >> (quotient + 1)) & ((1 << parameter) - 1))
        delta = -(unsigned // 2) - 1 if unsigned & 1 else unsigned // 2
        bit += quotient + 1 + parameter
        if linear:
            dot = sum(coefficients[index] * values[position - index - 1] for index in range(order))
            if abs(dot) > 2147483647:
                raise RuntimeError('Predictor sum overflow')
            values.append((dot // 16384 if dot >= 0 else -((-dot) // 16384)) + delta)
        else:
            for level in range(order - 1, -1, -1):
                differences[level] += delta
                delta = differences[level]
            values.append(differences[0])
    return values[start:start + count]


def decode(descriptors, words, table, sample):
    return decode_slice(descriptors, words, table, sample, 1)[0]


def header(descriptors, words):
    def array(name, values):
        return f'const opxy::u32 {name}[{len(values)}]={{\n' + ''.join(
            ','.join(f'0x{value:08x}u' for value in values[start:start + 12]) + ',\n'
            for start in range(0, len(values), 12)) + '};\n'
    zeros = [4 if not value else (value & -value).bit_length() - 1 for value in range(16)]
    prefix = 'const opxy::u8 bank_prefix_zeros[16]={' + ','.join(map(str, zeros)) + '};\n'
    return ('// Generated by pack_bank.py; exact bounded full-wave reconstruction.\n' + prefix +
            array('bank_blocks', descriptors) + array('bank_words', words))


def main():
    bank = authored_bank()
    descriptors, words, stats = pack(bank)
    for table in range(1450):
        if not np.array_equal(decode_slice(descriptors, words, table, 0, BLOCK), bank[table]):
            raise RuntimeError('Decoded bank differs from authored sound')
    output = header(descriptors, words)
    (FOLDER / 'bank_packed.h').write_text(output)
    report = dict(format='bounded-predictor-rice256/1', original_bytes=int(bank.size * 2),
                  packed_bytes=(len(descriptors) + len(words)) * 4 + 16, tables=1450, frames=256,
                  block_samples=BLOCK, seed_bits=14, coefficient_bits=18, predictor_shift=14,
                  predictor_absolute_sum_limit=15 * 16384, offset_bits=22,
                  predictors_sha256=hashlib.sha256((FOLDER / 'predictors.json').read_bytes()).hexdigest(),
                  decoded_sha256=hashlib.sha256(bank.astype('<i2').tobytes()).hexdigest(),
                  packed_header_sha256=hashlib.sha256(output.encode()).hexdigest(),
                  max_sample_error=0, runtime_wave_cache_bytes=0, runtime_resume_state_bytes=3456, runtime_allocation=False,
                  max_decoded_samples_per_slice=BLOCK, **stats)
    (FOLDER / 'packing-report.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
