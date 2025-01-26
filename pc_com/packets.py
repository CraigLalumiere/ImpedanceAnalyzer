import struct
from crc import calculate_crc
from messages.LogPrint_pb2 import LogPrint
from messages.CLIData_pb2 import CLIData
from messages.AddToPlot_pb2 import AddToPlot
from messages.DrawPlot_pb2 import DrawPlot
from messages.MessageType_pb2 import MessageType

message_from_id = {MessageType.LOG_PRINT: LogPrint,
                   MessageType.CLI_DATA: CLIData,
                   MessageType.ADD_TO_PLOT: AddToPlot,
                   MessageType.DRAW_PLOT: DrawPlot
                   }


def get_message_from_packet(packet):
    """
    Unpacks framed packet, confirms CRC is good, than returns decoded protobuf object
    """
    message = None

    # extract crc from packet
    # little endian, first 2 bytes
    packet_crc = struct.unpack('<H', packet[0:2])[0]

    # calculate packet crc
    crc_calc = calculate_crc(packet[2:])

    # check if crc provided by packet matches the calculated crc
    if packet_crc == crc_calc:
        # packet id follows CRC, the 3rd byte
        packet_id = struct.unpack('<B', packet[2:3])[0]

        try:
            message = message_from_id[packet_id]()
            message.ParseFromString(packet[3:])
        except KeyError:
            return None
    else:
        print('CRC fail!')

    return message


def build_packet_cli_data(data: bytes):
    packet_id = struct.pack('<B', MessageType.CLI_DATA)

    message_pb = CLIData()
    message_pb.msg = data
    message_bytes = message_pb.SerializeToString()

    packet_id_and_data = packet_id + message_bytes
    packet_crc = struct.pack('<H', calculate_crc(packet_id_and_data))
    packet = packet_crc + packet_id_and_data

    return packet





