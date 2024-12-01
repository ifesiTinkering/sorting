import random

# Define constants
FILE_NAME = "random_numbers.txt"
TARGET_SIZE_GB = 2
BYTES_PER_INT = 11  # Approx. size of a 32-bit integer and newline in text (10 digits + newline)
NUM_INTS = (TARGET_SIZE_GB * 1024 * 1024 * 1024) // BYTES_PER_INT

def generate_random_numbers(file_name, num_ints):
    with open(file_name, "w") as f:
        for _ in range(num_ints):
            # Generate a random 32-bit integer and write it as text
            f.write(f"{random.randint(-2**31, 2**31 - 1)}\n")

if __name__ == "__main__":
    print(f"Generating a file with approximately {TARGET_SIZE_GB} GB of random 32-bit integers...")
    generate_random_numbers(FILE_NAME, NUM_INTS)
    print(f"File '{FILE_NAME}' with {NUM_INTS} integers has been created.")
