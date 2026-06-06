# read_dump.py - универсальная версия
import struct
import time
import os
import sys
import struct

hash_to_word = {
    2349659949: "consciousness",
    1129629325: "awareness",
    162033611: "what",
    1853826712: "is",
    # ... добавьте остальные
}

def decode_meaning(encoded):
    parts = encoded.split('|')
    if len(parts) == 2:
        h1, h2 = int(parts[0]), int(parts[1])
        w1 = hash_to_word.get(h1, f"unknown_{h1}")
        w2 = hash_to_word.get(h2, f"unknown_{h2}")
        return f"{w1} + {w2}"
    return encoded

def read_string(f):
    length = struct.unpack('Q', f.read(8))[0]
    return f.read(length).decode('utf-8')

def read_vector_float(f):
    size = struct.unpack('Q', f.read(8))[0]
    if size == 0:
        return []
    data = struct.unpack(f'{size}f', f.read(size * 4))
    return list(data)

def read_metadata(f):
    meta_size = struct.unpack('Q', f.read(8))[0]
    metadata = {}
    for _ in range(meta_size):
        key_len = struct.unpack('Q', f.read(8))[0]
        key = f.read(key_len).decode('utf-8')
        val_len = struct.unpack('Q', f.read(8))[0]
        val = f.read(val_len).decode('utf-8')
        metadata[key] = val
    return metadata

# Ищем файл в текущей директории и родительской
filename = None
if os.path.exists('short_term.bin'):
    filename = 'short_term.bin'
elif os.path.exists('../short_term.bin'):
    filename = '../short_term.bin'
elif os.path.exists('dump/short_term.bin'):
    filename = 'dump/short_term.bin'
else:
    print("❌ short_term.bin not found!")
    print("Current directory:", os.getcwd())
    print("Files in current directory:", os.listdir('.'))
    sys.exit(1)

print(f"📂 Reading: {filename}\n")

with open(filename, 'rb') as f:
    size = struct.unpack('Q', f.read(8))[0]
    print(f"Records count: {size}\n")
    
    for i in range(min(size, 50)):
        try:
            timestamp = struct.unpack('Q', f.read(8))[0]
            component = read_string(f)
            type_str = read_string(f)
            data = read_vector_float(f)
            importance = struct.unpack('f', f.read(4))[0]
            metadata = read_metadata(f)
            
            print(f"--- Record {i} ---")
            print(f"  Timestamp: {timestamp} ({time.ctime(timestamp)})")
            print(f"  Component: {component}")
            print(f"  Type: {type_str}")
            print(f"  Importance: {importance}")
            print(f"  Data size: {len(data)}")
            if data and len(data) <= 20:
                # Если это текст, попробуем декодировать
                if all(32 <= x < 127 for x in data[:10]):
                    text = ''.join(chr(int(x)) for x in data if 32 <= x < 127)
                    print(f"  Data (text): {text}")
                else:
                    print(f"  Data: {data}")
            elif data:
                print(f"  Data (first 10): {data[:10]}...")
            if metadata:
                print(f"  Metadata: {metadata}")
            print()
        except Exception as e:
            print(f"Error reading record {i}: {e}")
            break