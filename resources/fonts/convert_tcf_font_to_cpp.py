import sys

def binary_to_text(input_file, output_file):
    try:
        with open(input_file, 'rb') as infile, open(output_file, 'w') as outfile:
            infile.read(3)  # Skip the first 3 bytes
            group_count = 0

            while True:
                group = []
                for _ in range(8):
                    byte = infile.read(1)
                    if not byte:
                        break
                    group.append(byte)

                if not group:
                    break

                if group_count > 0:
                    outfile.write(",\n")

                    if group_count % 8 == 0:
                        outfile.write("\n")
                    if group_count % 16 == 0:
                        outfile.write("\n")

                outfile.write("{ ")
                
                for i, byte in enumerate(group):
                    binary_representation = format(byte[0], '08b')
                    if i > 0:
                        outfile.write("  ")
                    if i < len(group) - 1:
                        outfile.write(f"0b{binary_representation},\n")
                    else:
                        outfile.write(f"0b{binary_representation}")

                outfile.write(" }")

                group_count += 1

            print(f"Conversion complete. Output written to {output_file}.")
    except FileNotFoundError:
        print(f"Error: The file {input_file} does not exist.")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python script.py <input_file> <output_file>")
    else:
        input_file = sys.argv[1]
        output_file = sys.argv[2]
        binary_to_text(input_file, output_file)