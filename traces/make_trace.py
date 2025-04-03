import random

def generate_write_intensive_trace(num_lines=7000000, num_devices=16):
    """Generates a write-intensive trace file.

    Args:
        num_lines: The number of lines to generate in the trace.
        num_devices: The number of devices to simulate.

    Returns:
        A string containing the generated trace data.
    """

    trace_data = ""
    timestamp = 930000000  # start timestamp

    for _ in range(num_lines):
        timestamp += random.randint(1, 1000)  # 1 ~ 1000 microsecond interval

        device_number = random.randint(0, num_devices)

        starting_sector = random.randint(0, 1000000000)  #0 ~ 10

        request_size = random.choice([16,32])  # 16 / 32 selection

        request_type = 0  # Write / Read half-half

        trace_data += "%d %d %d %d %d\n" % (timestamp, device_number, starting_sector, request_size, request_type)

    return trace_data

trace = generate_write_intensive_trace()
with open("write_intensive.trace", "w") as f:
    f.write(trace)

print("write_intensive.trace complete!")

