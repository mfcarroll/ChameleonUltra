import struct
import ctypes
from typing import Union

import chameleon_com
from chameleon_utils import expect_response, reconstruct_full_nt, parity_to_str
from chameleon_enum import Command, SlotNumber, Status, TagSenseType, TagSpecificType
from chameleon_enum import ButtonPressFunction, ButtonType, MifareClassicDarksideStatus
from chameleon_enum import MfcKeyType, MfcValueBlockOperator

CURRENT_VERSION_SETTINGS = 6

# ⛔⛔ THESE TWO LINES LOCKED TAGS FOR YEARS, AND THE OLD VALUES ARE KEPT BELOW ON PURPOSE.
#
# `new_key` is the password a write SETS. It used to be 20206666 here — and 51243648 in the
# function signatures of indala/indala224/gproxii/awid — so which password your tag ended up
# with depended on which command you ran, and neither value was documented anywhere. Combined
# with T5577_PWD being set in every config constant, **any write silently password-protected
# the tag**, and a tag locked by `lf hid prox write` could not be opened by `lf gproxii write`,
# by the Proxmark, or by anything else (C325).
#
# ⭐ `new_key` is now ZERO, which means "do not set a password". The firmware only ORs
# T5577_PWD into block 0 when a non-zero key is supplied, so a plain write leaves the tag open.
# Pass a key explicitly if you actually want protection.
#
# ⭐ `old_keys` is the list of passwords a tag might ALREADY be locked behind, tried in order so
# previously-protected tags can still be written. 20206666 is first because it is what every
# `lf hid prox write` from the old code left behind — its absence from this list is exactly why
# a GProxII write could not open an HID-locked tag.
new_key = b'\x00\x00\x00\x00'
old_keys = [b'\x20\x20\x66\x66', b'\x51\x24\x36\x48', b'\x19\x92\x04\x27']


LF_SNIFF_CHUNK_BYTES = 4000   # keep in step with LF_SNIFF_CHUNK_BYTES in firmware


class ChameleonCMD:
    """
        Chameleon cmd function
    """

    def __init__(self, chameleon: chameleon_com.ChameleonCom):
        """
        :param chameleon: chameleon instance, @see chameleon_device.Chameleon
        """
        self.device = chameleon

    @expect_response(Status.SUCCESS)
    def get_app_version(self):
        """
            Get firmware version number(application)
        """
        resp = self.device.send_cmd_sync(Command.GET_APP_VERSION)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('!BB', resp.data)
        # older protocol, must upgrade!
        if resp.status == 0 and resp.data == b'\x00\x01':
            print("Chameleon does not understand new protocol. Please update firmware")
            return chameleon_com.Response(cmd=Command.GET_APP_VERSION,
                                          status=Status.NOT_IMPLEMENTED)
        return resp

    @expect_response(Status.SUCCESS)
    def get_device_chip_id(self):
        """
            Get device chip id
        """
        resp = self.device.send_cmd_sync(Command.GET_DEVICE_CHIP_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data.hex()
        return resp

    @expect_response(Status.SUCCESS)
    def get_device_address(self):
        """
            Get device address
        """
        resp = self.device.send_cmd_sync(Command.GET_DEVICE_ADDRESS)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data.hex()
        return resp

    @expect_response(Status.SUCCESS)
    def get_git_version(self):
        resp = self.device.send_cmd_sync(Command.GET_GIT_VERSION)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data.decode('utf-8')
        return resp

    @expect_response(Status.SUCCESS)
    def get_device_mode(self):
        resp = self.device.send_cmd_sync(Command.GET_DEVICE_MODE)
        if resp.status == Status.SUCCESS:
            resp.parsed, = struct.unpack('!?', resp.data)
        return resp

    def is_device_reader_mode(self) -> bool:
        """
            Get device mode, reader or tag.

        :return: True is reader mode, else tag mode
        """
        return self.get_device_mode()

    # Note: Will return NOT_IMPLEMENTED if one tries to set reader mode on Lite
    @expect_response(Status.SUCCESS)
    def change_device_mode(self, mode):
        data = struct.pack('!B', mode)
        return self.device.send_cmd_sync(Command.CHANGE_DEVICE_MODE, data)

    def set_device_reader_mode(self, reader_mode: bool = True):
        """
            Change device mode, reader or tag.

        :param reader_mode: True if reader mode, False if tag mode.
        :return:
        """
        self.change_device_mode(reader_mode)

    @expect_response(Status.HF_TAG_OK)
    def hf14a_scan(self):
        """
        14a tags in the scanning field.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.HF14A_SCAN)
        if resp.status == Status.HF_TAG_OK:
            # uidlen[1]|uid[uidlen]|atqa[2]|sak[1]|atslen[1]|ats[atslen]
            offset = 0
            data = []
            while offset < len(resp.data):
                uidlen, = struct.unpack_from('!B', resp.data, offset)
                offset += struct.calcsize('!B')
                uid, atqa, sak, atslen = struct.unpack_from(f'!{uidlen}s2s1sB', resp.data, offset)
                offset += struct.calcsize(f'!{uidlen}s2s1sB')
                ats, = struct.unpack_from(f'!{atslen}s', resp.data, offset)
                offset += struct.calcsize(f'!{atslen}s')
                data.append({'uid': uid, 'atqa': atqa, 'sak': sak, 'ats': ats})
            resp.parsed = data
        return resp

    def mf1_detect_support(self):
        """
        Detect whether it is mifare classic tag.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.MF1_DETECT_SUPPORT)
        return resp.status == Status.HF_TAG_OK

    @expect_response(Status.HF_TAG_OK)
    def mf1_detect_prng(self):
        """
        Detect mifare Class of classic nt vulnerabilities.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.MF1_DETECT_PRNG)
        if resp.status == Status.HF_TAG_OK:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_detect_nt_dist(self, block_known, type_known, key_known):
        """
        Detect the random number distance of the card.

        :return:
        """
        data = struct.pack('!BB6s', type_known, block_known, key_known)
        resp = self.device.send_cmd_sync(Command.MF1_DETECT_NT_DIST, data)
        if resp.status == Status.HF_TAG_OK:
            uid, dist = struct.unpack('!II', resp.data)
            resp.parsed = {'uid': uid, 'dist': dist}
        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_nested_acquire(self, block_known, type_known, key_known, block_target, type_target):
        """
        Collect the key NT parameters needed for Nested decryption
        :return:
        """
        data = struct.pack('!BB6sBB', type_known, block_known, key_known, type_target, block_target)
        resp = self.device.send_cmd_sync(Command.MF1_NESTED_ACQUIRE, data)
        if resp.status == Status.HF_TAG_OK:
            resp.parsed = [{'nt': nt, 'nt_enc': nt_enc, 'par': par}
                           for nt, nt_enc, par in struct.iter_unpack('!IIB', resp.data)]
        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_darkside_acquire(self, block_target, type_target, first_recover: Union[int, bool], sync_max):
        """
        Collect the key parameters needed for Darkside decryption.

        :param block_target:
        :param type_target:
        :param first_recover:
        :param sync_max:
        :return:
        """
        data = struct.pack('!BBBB', type_target, block_target, first_recover, sync_max)
        resp = self.device.send_cmd_sync(Command.MF1_DARKSIDE_ACQUIRE, data, timeout=sync_max * 10)
        if resp.status == Status.HF_TAG_OK:
            if resp.data[0] == MifareClassicDarksideStatus.OK:
                darkside_status, uid, nt1, par, ks1, nr, ar = struct.unpack('!BIIQQII', resp.data)
                resp.parsed = (darkside_status, {'uid': uid, 'nt1': nt1, 'par': par, 'ks1': ks1, 'nr': nr, 'ar': ar})
            else:
                resp.parsed = (resp.data[0],)
        return resp

    @expect_response([Status.HF_TAG_OK, Status.MF_ERR_AUTH])
    def mf1_auth_one_key_block(self, block, type_value: MfcKeyType, key):
        """
        Verify the mf1 key, only verify the specified type of key for a single sector.

        :param block:
        :param type_value:
        :param key:
        :return:
        """
        data = struct.pack('!BB6s', type_value, block, key)
        resp = self.device.send_cmd_sync(Command.MF1_AUTH_ONE_KEY_BLOCK, data)
        resp.parsed = resp.status == Status.HF_TAG_OK
        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_read_one_block(self, block, type_value: MfcKeyType, key):
        """
        Read one mf1 block.

        :param block:
        :param type_value:
        :param key:
        :return:
        """
        data = struct.pack('!BB6s', type_value, block, key)
        resp = self.device.send_cmd_sync(Command.MF1_READ_ONE_BLOCK, data)
        resp.parsed = resp.data
        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_write_one_block(self, block, type_value: MfcKeyType, key, block_data):
        """
        Write mf1 single block.

        :param block:
        :param type_value:
        :param key:
        :param block_data:
        :return:
        """
        data = struct.pack('!BB6s16s', type_value, block, key, block_data)
        resp = self.device.send_cmd_sync(Command.MF1_WRITE_ONE_BLOCK, data)
        resp.parsed = resp.status == Status.HF_TAG_OK
        return resp

    @expect_response(Status.HF_TAG_OK)
    def hf14a_scan_keep(self):
        """
        Scan ISO14443-A tag with full select + RATS, keeping field alive.

        Identical to hf14a_scan but does NOT tear down the RF field afterward.
        The card remains powered and in ISO14443-4 T=CL state so subsequent
        hf14a_raw calls can exchange APDUs without re-selecting.
        """
        resp = self.device.send_cmd_sync(Command.HF14A_SCAN_KEEP)
        if resp.status == Status.HF_TAG_OK:
            offset = 0
            data = []
            while offset < len(resp.data):
                uidlen, = struct.unpack_from('!B', resp.data, offset)
                offset += 1
                uid, atqa, sak, atslen = struct.unpack_from(
                    f'!{uidlen}s2s1sB', resp.data, offset)
                offset += struct.calcsize(f'!{uidlen}s2s1sB')
                ats, = struct.unpack_from(f'!{atslen}s', resp.data, offset)
                offset += atslen
                data.append({'uid': uid, 'atqa': atqa, 'sak': sak, 'ats': ats})
            resp.parsed = data
        return resp

    def hf14a_4_set_anti_coll(self, uid: bytes, atqa: bytes, sak: int, ats: bytes):
        """
        Set UID / ATQA / SAK / ATS for the active HF14A_4 slot.

        :param uid:  UID bytes (4 or 7 bytes)
        :param atqa: ATQA 2 bytes (wire order, e.g. b'\x04\x00' for ATQA 00 04)
        :param sak:  SAK byte value (int), use 0x20 for ISO14443-4
        :param ats:  ATS bytes (without CRC)
        """
        uid_size = len(uid)
        payload = (bytes([uid_size]) + bytes(uid) + bytes(atqa) +
                   bytes([sak]) + bytes([len(ats)]) + bytes(ats))
        return self.device.send_cmd_sync(Command.HF14A_4_SET_ANTI_COLL, payload)

    def hf14a_4_apdu_recv(self):
        """
        Non-blocking poll for a pending APDU from the ISO14443-4 T=CL stack.

        Returns immediately: STATUS_SUCCESS + APDU bytes if one is pending,
        STATUS_HF_TAG_NO if no APDU is waiting.  Call in a tight loop from
        the host side for relay/capture use cases.
        """
        return self.device.send_cmd_sync(Command.HF14A_4_APDU_RECV, b'', timeout=2)

    def hf14a_4_apdu_send(self, resp: bytes):
        """Send an APDU response to the ISO14443-4 T=CL stack."""
        payload = bytes([(len(resp) >> 8) & 0xFF, len(resp) & 0xFF]) + bytes(resp)
        return self.device.send_cmd_sync(Command.HF14A_4_APDU_SEND, payload)

    def hf14a_4_add_static_response(self, cmd: bytes, resp: bytes):
        """
        Add a static APDU command→response pair to the HF14A_4 slot.

        The firmware will automatically reply with resp whenever it receives
        an APDU whose first len(cmd) bytes match cmd, without USB involvement.
        Must be called before hw mode -e.
        """
        rlen = len(resp)
        payload = bytes([len(cmd)]) + bytes(cmd) + bytes([(rlen >> 8) & 0xFF, rlen & 0xFF]) + bytes(resp)
        return self.device.send_cmd_sync(Command.HF14A_4_STATIC_RESP, payload)

    def hf14a_4_reader_apdu(self, apdu: bytes):
        """
        Select card (with RATS) and send one ISO14443-4 T=CL APDU in a single
        firmware call — avoiding the USB round-trip gap that would depower the card.

        :param apdu: raw APDU bytes (no PCB wrapping needed)
        :return: response object with resp.data = APDU response bytes (no PCB/CRC)
        """
        return self.device.send_cmd_sync(
            Command.HF14A_4_READER_APDU, bytes(apdu), timeout=3)

    def hf14a_4_emv_scan(self):
        """
        Full EMV card scan in a single firmware call.

        The firmware performs the complete sequence (field cycle, select, RATS,
        PPSE, SELECT AID, GPO, READ RECORDs) without returning to the host
        between APDUs, avoiding the field-drop issue with separate calls.

        Response format:
            uid_len(1) uid(n) atqa(2) sak(1) ats_len(1) ats(m)
            num_apdus(1)
            for each APDU pair:
                cmd_len(1) cmd(n) resp_len_le(2) resp(m)
        """
        resp = self.device.send_cmd_sync(Command.HF14A_4_EMV_SCAN, b'', timeout=10)
        return resp

    def hf14a_4_clear_static_responses(self):
        """Clear all static APDU responses from the active HF14A_4 slot."""
        return self.device.send_cmd_sync(Command.HF14A_4_STATIC_RESP, b'\x00')

    def hf14a_raw(self, options, resp_timeout_ms=100, data=[], bitlen=None):
        """
        Send raw cmd to 14a tag.

        :param options:
        :param resp_timeout_ms:
        :param data:
        :param bit_owned_by_the_last_byte:
        :return:
        """

        class CStruct(ctypes.BigEndianStructure):
            _fields_ = [
                ("activate_rf_field", ctypes.c_uint8, 1),
                ("wait_response", ctypes.c_uint8, 1),
                ("append_crc", ctypes.c_uint8, 1),
                ("auto_select", ctypes.c_uint8, 1),
                ("keep_rf_field", ctypes.c_uint8, 1),
                ("check_response_crc", ctypes.c_uint8, 1),
                ("reserved", ctypes.c_uint8, 2),
            ]

        cs = CStruct()
        cs.activate_rf_field = options['activate_rf_field']
        cs.wait_response = options['wait_response']
        cs.append_crc = options['append_crc']
        cs.auto_select = options['auto_select']
        cs.keep_rf_field = options['keep_rf_field']
        cs.check_response_crc = options['check_response_crc']

        if bitlen is None:
            bitlen = len(data) * 8  # bits = bytes * 8(bit)
        else:
            if len(data) == 0:
                raise ValueError(f'bitlen={bitlen} but missing data')
            if not ((len(data) - 1) * 8 < bitlen <= len(data) * 8):
                raise ValueError(f'bitlen={bitlen} incompatible with provided data ({len(data)} bytes), '
                                 f'must be between {((len(data) - 1) * 8)+1} and {len(data) * 8} included')

        data = bytes(cs)+struct.pack(f'!HH{len(data)}s', resp_timeout_ms, bitlen, bytearray(data))
        resp = self.device.send_cmd_sync(Command.HF14A_RAW, data, timeout=(resp_timeout_ms // 1000) + 1)
        return resp.data

    @expect_response(Status.HF_TAG_OK)
    def mf1_manipulate_value_block(self, src_block, src_type: MfcKeyType, src_key, operator: MfcValueBlockOperator, operand, dst_block, dst_type: MfcKeyType, dst_key):
        """
        1. Increment: increments value from source block and write to dest block
        2. Decrement: decrements value from source block and write to dest block
        3. Restore: copy value from source block and write to dest block


        :param src_block:
        :param src_type:
        :param src_key:
        :param operator:
        :param operand:
        :param dst_block:
        :param dst_type:
        :param dst_key:
        :return:
        """
        data = struct.pack('!BB6sBiBB6s', src_type, src_block, src_key, operator, operand, dst_type, dst_block, dst_key)
        resp = self.device.send_cmd_sync(Command.MF1_MANIPULATE_VALUE_BLOCK, data)
        resp.parsed = resp.status == Status.HF_TAG_OK
        return resp

    @expect_response([Status.HF_TAG_OK, Status.HF_TAG_NO])
    def mf1_check_keys_of_sectors(self, mask: bytes, keys: list[bytes]):
        """
        Check keys of sectors.
        :return:
        """
        if len(mask) != 10:
            raise ValueError("len(mask) should be 10")
        if len(keys) < 1 or len(keys) > 83:
            raise ValueError("Invalid len(keys)")
        data = struct.pack(f'!10s{6*len(keys)}s', mask, b''.join(keys))

        bitsCnt = 80  # maximum sectorKey_to_be_checked
        for b in mask:
            while b > 0:
                [bitsCnt, b] = [bitsCnt - (b & 0b1), b >> 1]
        if bitsCnt < 1:
            # All sectorKey is masked
            return chameleon_com.Response(
                cmd=Command.MF1_CHECK_KEYS_OF_SECTORS,
                status=Status.HF_TAG_OK,
                parsed={'status': Status.HF_TAG_OK},
            )
        # base timeout: 1s
        # auth: len(keys) * sectorKey_to_be_checked * 0.1s
        # read keyB from trailer block: 0.1s
        timeout = 1 + (bitsCnt + 1) * len(keys) * 0.1

        resp = self.device.send_cmd_sync(Command.MF1_CHECK_KEYS_OF_SECTORS, data, timeout=timeout)
        resp.parsed = {'status': resp.status}
        if len(resp.data) == 490:
            found = ''.join([format(i, '08b') for i in resp.data[0:10]])
            # print(f'{found = }')
            resp.parsed.update({
                'found': resp.data[0:10],
                'sectorKeys': {k: resp.data[6 * k + 10:6 * k + 16] for k, v in enumerate(found) if v == '1'}
            })
        return resp

    @expect_response([Status.HF_TAG_OK, Status.HF_TAG_NO, Status.MF_ERR_AUTH])
    def mf1_check_keys_on_block(self, block: int, key_type: int, keys: list[bytes]):
        if key_type not in [0x60, 0x61]:
            raise ValueError("Wrong key type")
        if len(keys) < 1 or len(keys) > 83:
            raise ValueError("Invalid len(keys)")
        data = struct.pack(f'!BBB{6*len(keys)}s', block, key_type, len(keys), b''.join(keys))

        resp = self.device.send_cmd_sync(Command.MF1_CHECK_KEYS_ON_BLOCK, data, timeout=10)

        if resp.status == Status.HF_TAG_OK and len(resp.data) == 7:
            found, key = struct.unpack('!B6s', resp.data)
            if found:
                resp.parsed = key

        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_static_nested_acquire(self, block_known, type_known, key_known, block_target, type_target):
        """
        Collect the key NT parameters needed for StaticNested decryption
        :return:
        """
        data = struct.pack('!BB6sBB', type_known, block_known, key_known, type_target, block_target)
        resp = self.device.send_cmd_sync(Command.MF1_STATIC_NESTED_ACQUIRE, data)
        if resp.status == Status.HF_TAG_OK:
            resp.parsed = {
                'uid': struct.unpack('!I', resp.data[0:4])[0],
                'nts': [
                    {
                        'nt': nt,
                        'nt_enc': nt_enc
                    } for nt, nt_enc in struct.iter_unpack('!II', resp.data[4:])
                ]
            }
        return resp

    @expect_response(Status.HF_TAG_OK)
    def mf1_hard_nested_acquire(self, slow, block_known, type_known, key_known, block_target, type_target):
        """
        Collect the NT_ENC list for HardNested decryption
        :return:
        """
        data = struct.pack('!BBB6sBB', slow, type_known, block_known, key_known, type_target, block_target)
        resp = self.device.send_cmd_sync(Command.MF1_HARDNESTED_ACQUIRE, data, timeout=30)
        if resp.status == Status.HF_TAG_OK:
            resp.parsed = resp.data  # we can return the raw nonces bytes
        return resp

    @expect_response([Status.HF_TAG_OK, Status.HF_TAG_NO])
    def mf1_static_encrypted_nested_acquire(self, backdoor_key, sector_count, starting_sector):
        data = struct.pack('!6sBB', backdoor_key, sector_count, starting_sector)
        resp = self.device.send_cmd_sync(Command.MF1_ENC_NESTED_ACQUIRE, data, timeout=30)
        if resp.status == Status.HF_TAG_OK:
            resp.parsed = {
                'uid': struct.unpack('!I', resp.data[0:4])[0],
                'nts': {
                    'a': [],
                    'b': []
                }
            }

            i = 4

            while i < len(resp.data):
                resp.parsed['nts']['a'].append(
                    {
                        'nt': reconstruct_full_nt(resp.data, i),
                        'nt_enc': int.from_bytes(resp.data[i + 3: i + 7], byteorder='big'),
                        'parity': parity_to_str(resp.data[i + 2])
                    }
                )

                resp.parsed['nts']['b'].append(
                    {
                        'nt': reconstruct_full_nt(resp.data, i + 7),
                        'nt_enc': int.from_bytes(resp.data[i + 10: i + 14], byteorder='big'),
                        'parity': parity_to_str(resp.data[i + 9])
                    }
                )

                i += 14
        return resp

    def hf14a_sniff(self, timeout_ms: int = 5000):
        """
        Capture ISO14443A reader frames while CU acts as a tag emulator.

        The firmware installs a sniff callback into the HF14A stack for the
        requested duration, then returns all captured frames packed as:
          [2 bytes: bit count, big-endian] [N bytes: frame data, ceil(bits/8)] ...

        :param timeout_ms: Listen duration in ms (1-30000, default 5000)
        :return: Raw response — check .status and .data
        """
        timeout_ms = max(1, min(30000, timeout_ms))
        payload = bytes([(timeout_ms >> 8) & 0xFF, timeout_ms & 0xFF])
        timeout_s = (timeout_ms // 1000) + 5
        return self.device.send_cmd_sync(Command.HF14A_SNIFF, payload, timeout=timeout_s)

    def hf14a_auth_trace(self, block: int, key_type: int, key: bytes, timeout_ms: int = 5000):
        """
        Run a full reader-side ISO14443A + MIFARE Classic Crypto1 auth flow
        against a real card and return every wire frame for inspection.

        The firmware polls for a tag in the field for up to `timeout_ms`
        milliseconds, then performs anticoll + SELECT + (optional RATS) +
        AUTH and packs all frames — synthesized anticoll plus the live
        AUTH/NT/NR||AR/AT — into the same buffer format used by hf14a_sniff:
          [2 bytes: bit count, big-endian] [N bytes: frame data, ceil(bits/8)] ...
        Bit 15 of the bit-count header: 0 = reader→card, 1 = card→reader.

        :param block:      target block number (0-255)
        :param key_type:   0x60 (Key A) or 0x61 (Key B)
        :param key:        6-byte sector key
        :param timeout_ms: tag-presence polling timeout in ms (1-30000)
        :return: Raw response — check .status and .data
        """
        if key_type not in (0x60, 0x61):
            raise ValueError("key_type must be 0x60 (Key A) or 0x61 (Key B)")
        if len(key) != 6:
            raise ValueError("key must be exactly 6 bytes")
        timeout_ms = max(1, min(30000, int(timeout_ms)))
        payload = (
            bytes([key_type, block & 0xFF])
            + bytes(key)
            + bytes([(timeout_ms >> 8) & 0xFF, timeout_ms & 0xFF])
        )
        # Add a couple of seconds of slack on top of the device-side polling
        # window so the USB/BLE round-trip doesn't time out before firmware
        # gives up on its own.
        return self.device.send_cmd_sync(Command.HF14A_AUTH_TRACE, payload, timeout=(timeout_ms // 1000) + 3)

    @expect_response(Status.SUCCESS)
    def hf14a_get_config(self):
        """
        Get hf 14a config

        :return:
        """
        resp = self.device.send_cmd_sync(Command.HF14A_GET_CONFIG)
        if resp.status == Status.SUCCESS:
            bcc, cl2, cl3, rats = struct.unpack('!bbbb', resp.data)
            resp.parsed = {'bcc': bcc,
                           'cl2': cl2,
                           'cl3': cl3,
                           'rats': rats}
        return resp

    @expect_response(Status.SUCCESS)
    def hf14a_set_config(self, data):
        """
        Set hf 14a config

        :return:
        """
        data = struct.pack('!bbbb', data['bcc'], data['cl2'], data['cl3'], data['rats'])
        return self.device.send_cmd_sync(Command.HF14A_SET_CONFIG, data)

    @expect_response(Status.LF_TAG_OK)
    def em410x_scan(self):
        """
        Read the card number of EM410X.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.EM410X_SCAN)
        if resp.status == Status.LF_TAG_OK:
            tag_type = struct.unpack('!H', resp.data[:2])[0]
            if tag_type == TagSpecificType.EM410X_ELECTRA:
                fmt = '!H13s'
            else:
                fmt = '!H5s'
            resp.parsed = struct.unpack(fmt, resp.data[:struct.calcsize(fmt)])  # tag type + uid
        return resp

    @expect_response(Status.LF_TAG_OK)
    def em410x_write_to_t55xx(self, id_bytes: bytes):
        """
        Write EM410X card number into T55XX.

        :param id_bytes: ID card number
        :return:
        """
        if len(id_bytes) == 5:
            data = struct.pack(f'!5s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
            return self.device.send_cmd_sync(Command.EM410X_WRITE_TO_T55XX, data)
        if len(id_bytes) == 13:
            data = struct.pack(f'!13s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
            return self.device.send_cmd_sync(Command.EM410X_ELECTRA_WRITE_TO_T55XX, data)
        raise ValueError("The id bytes length must equal 5 (EM410X) or 13 (Electra)")

    @expect_response(Status.LF_TAG_OK)
    def hidprox_scan(self, format: int):
        """
        Read the length, facility code and card number of HID Prox.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.HIDPROX_SCAN, struct.pack('!B', format))
        if resp.status == Status.LF_TAG_OK:
            # ⭐ Bytes 13..15 carry the AMBIGUITY the reader used to swallow: how many other
            # layouts of this bit length also accept the frame, and the first two by format id.
            # The firmware enumerates (it owns `formats[]`); the host only maps ids to names.
            # ⚠ Older firmware left these three bytes zero, which reads as "no other match" —
            # the honest degradation, and the reason they were chosen over a longer payload.
            fields = struct.unpack('>BIBIBH', resp.data[:13])
            others = tuple(b for b in resp.data[14:16] if b)
            resp.parsed = fields + (resp.data[13], others)
        return resp

    @expect_response(Status.LF_TAG_OK)
    def hidprox_write_to_t55xx(self, id_bytes: bytes):
        """
        Write HID Prox card number into T55XX.

        :param id_bytes: ID card number
        :return:
        """
        if len(id_bytes) != 13:
            raise ValueError("The id bytes length must equal 13")
        data = struct.pack(f'!13s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.HIDPROX_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def ioprox_scan(self):
        """
        Read ioProx (XSF): version, facility, number, raw.
        """
        resp = self.device.send_cmd_sync(Command.IOPROX_SCAN)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = struct.unpack(">BBH8sBBBB", resp.data[:16])
        return resp

    @expect_response(Status.LF_TAG_OK)
    def lf_t55xx_write_block(self, block: int, word: int, use_pwd: bool = False,
                             pwd: int = 0, page1: bool = False):
        """Write one 32-bit T5577 block.

        The firmware handler has existed since before this work (DATA_CMD_LF_T55XX_WRITE,
        app_cmd.c) with no host binding at all — every write went through a
        protocol-specific helper, so there was no way to put an arbitrary word on a tag.
        """
        if not 0 <= block <= (3 if page1 else 7):
            raise ValueError("block out of range for the selected page")
        data = struct.pack('!BIBIB', block, word & 0xFFFFFFFF, 1 if use_pwd else 0,
                           pwd & 0xFFFFFFFF, 1 if page1 else 0)
        return self.device.send_cmd_sync(Command.LF_T55XX_WRITE, data)

    @expect_response(Status.LF_TAG_OK)
    def indala_write_to_t55xx(self, raw8: bytes, new_key: bytes = None,
                              old_keys: list = None):
        """Write a raw 64-bit Indala frame onto a T55xx tag (PSK1, RF/32).

        ⚠ Returns LF_TAG_OK regardless — a T5577 does not acknowledge a write. This says
        what was transmitted, not what landed. Read the tag back.
        """
        if len(raw8) != 8:
            raise ValueError("The raw frame must be exactly 8 bytes")
        new_key = new_key if new_key is not None else globals()["new_key"]
        old_keys = old_keys or globals()["old_keys"]
        data = struct.pack(f'!8s4s{4*len(old_keys)}s', raw8, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.INDALA_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def indala224_write_to_t55xx(self, raw28: bytes, new_key: bytes = None,
                                 old_keys: list = None):
        """Write a raw 224-bit Indala frame onto a T55xx tag.

        ⛔ The tag is configured PSK2, not PSK1 — see T5577_INDALA224_CONFIG and C99. It
        occupies all seven data blocks of page 0, so it cannot carry a password.

        ⚠ Returns LF_TAG_OK regardless — a T5577 does not acknowledge a write. This says
        what was transmitted, not what landed. Read the tag back.
        """
        if len(raw28) != 28:
            raise ValueError("The raw frame must be exactly 28 bytes")
        new_key = new_key if new_key is not None else globals()["new_key"]
        old_keys = old_keys or globals()["old_keys"]
        data = struct.pack(f'!28s4s{4*len(old_keys)}s', raw28, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.INDALA224_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def indala_scan(self):
        """
        Read an Indala credential (PSK1, RF/32, fc/2 subcarrier).

        ⚠ SLOWER THAN THE OTHER LF SCANS, by design. Indala's subcarrier lands at exactly
        Nyquist for the carrier-locked sampler, so the firmware demodulates whole
        4096-sample captures at a rotating sample phase and returns only once two of them
        agree. A capture is ~35ms and the measured median is 2 of them; the 95th
        percentile is 5.

        Returns (id, fc, csn, flags, phase, offset, tries) where id is the raw 64-bit frame,
        flags bit2 is "the Wiegand-26 parity checks out" and bits 1..0 are the parity bits
        themselves, and phase/offset say where in the carrier cycle the read came from.
        """
        # ⚠ NOT the 3s default. INDALA_READ_TIMEOUT_MS is 3000ms on the device and the
        # budget is only checked BETWEEN captures, so a read can land just past 3s and the
        # host would time out on a command that actually succeeded — which it did, and it
        # looked exactly like a successful read in the output.
        resp = self.device.send_cmd_sync(Command.INDALA_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = struct.unpack(">8sBHBBBB1x", resp.data[:16])
        return resp

    @expect_response(Status.SUCCESS)
    def lf_emu_debug(self):
        """
        ⚠ §3 instrumentation: read the LF emulation state in one shot.

        Changing a slot's LF tag type kills emulation until a power cycle (C126), and every
        test of that costs a power cycle — so this returns everything at once rather than
        making us spend a cycle per question. ⛔ Remove with the firmware side.

        Returns a dict: sense_state, emulating, tag_type, pwm_clk, pwm_inits, playbacks,
        hfclk_balance, hfclk_running, have_seq.
        """
        resp = self.device.send_cmd_sync(Command.LF_EMU_DEBUG)
        if resp.status == Status.SUCCESS:
            d = resp.data
            resp.parsed = {
                "sense_state": d[0], "emulating": d[1],
                "tag_type": (d[2] << 8) | d[3],
                "pwm_clk": d[4],
                "pwm_inits": (d[5] << 8) | d[6],
                "playbacks": (d[7] << 8) | d[8],
                "hfclk_balance": d[9], "hfclk_running": d[10], "have_seq": d[11],
                "frames_per_burst": (d[12] << 8) | d[13],
            }
        return resp

    @expect_response(Status.SUCCESS)
    def lf_radio_debug(self):
        """
        ⛔ C148 instrumentation: read the LF RADIO's drive value and PWM0's live registers.

        The drive control works from a fresh boot and goes inert partway through a session.
        Every test of that so far has been behavioural — inferring the peripheral's state from
        a decode rate — which produced four wrong explanations in one session. This reads the
        state directly, and it splits the question in two: if `drive` is wrong the RAM the PWM
        reads by DMA is being clobbered; if `drive` is right the peripheral is ignoring it, and
        `ptr_is_ours` says whether PWM0 is even pointed at our sequence any more.

        ⚠ lf_tag_em.c drives NRFX_PWM_INSTANCE(0) as well — the same peripheral, with its own
        separate belief about ownership. That is the leading suspect. ⛔ Remove with the
        firmware side.
        """
        resp = self.device.send_cmd_sync(Command.LF_RADIO_DEBUG)
        d = resp.data
        resp.parsed = {
            "drive": d[0], "reader_inited": d[1],
            "countertop": (d[2] << 8) | d[3], "prescaler": d[4],
            "decoder_load": d[5] & 0x07, "decoder_mode": d[6],
            "enable": d[7], "seq0_cnt": (d[8] << 8) | d[9],
            "ptr_is_ours": d[10],
            "seq0_ptr": (d[11] << 24) | (d[12] << 16) | (d[13] << 8) | d[14],
            "pwm_mode": d[15],
            "drive_at_start": d[16], "ptr_ours_at_start": d[17],
            "starts": (d[18] << 8) | d[19],
        }
        return resp


    @expect_response(Status.LF_TAG_OK)
    def indala224_scan(self):
        """
        Read a 224-bit Indala credential (PSK1, RF/32, fc/2 subcarrier).

        ⚠ Slower than the 64-bit scans: a 224-bit frame needs a 14336-sample capture, 114ms
        against 33ms, so the device's 3s budget buys about a quarter of the attempts.

        ⭐ Returns the raw 28-byte frame and nothing derived from it. There is no agreed
        facility-code layout for 224-bit Indala the way there is for format 26, so the device
        does not invent one.

        Returns (raw28, phase, offset, tries).
        """
        resp = self.device.send_cmd_sync(Command.INDALA224_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = struct.unpack(">28sBBB1x", resp.data[:32])
        return resp

    @expect_response(Status.LF_TAG_OK)
    def idteck_scan(self):
        """
        Read an IDTECK credential (PSK1, RF/32, fc/2 subcarrier).

        ⭐ The same physical layer as Indala and the same shared capture engine, so the same
        timing applies — see indala_scan for why this needs a 10s host timeout against a 3s
        device budget.

        Returns (id, chksum, card, phase, offset, tries) where id is the raw 64-bit frame
        (the first four bytes are always "IDTK") and card is the 24-bit card number.
        """
        resp = self.device.send_cmd_sync(Command.IDTECK_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            raw, chk, c_hi, c_mid, c_lo, phase, offset, tries = struct.unpack(
                ">8sBBBBBBB1x", resp.data[:16])
            card = (c_hi << 16) | (c_mid << 8) | c_lo
            resp.parsed = (raw, chk, card, phase, offset, tries)
        return resp

    @expect_response(Status.LF_TAG_OK)
    def keri_scan(self):
        """
        Read a Keri credential (PSK1, RF/32, fc/2 subcarrier).

        ⭐ Bit-identical air layer to Indala and IDTECK — same capture engine, same
        demodulator, same 10s host timeout against the 3s device budget. Only the 33-bit
        preamble and the payload interpretation differ.

        Returns (raw, internal_id, fc, cn, phase, offset, tries) where raw is the full
        64-bit frame — preamble `E0000000` then the internal id — and fc/cn are the
        de-scrambled facility code and card number.

        ⚠ fc and cn are ADVISORY. A tag written with an internal id directly (`lf keri
        clone -t i`) has no facility/card structure, so its de-scramble is meaningless.
        """
        resp = self.device.send_cmd_sync(Command.KERI_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            raw, fc, c_hi, c_mid, c_lo, phase, offset, tries = struct.unpack(
                ">8sBBBBBBB1x", resp.data[:16])
            internal_id = int.from_bytes(raw[4:8], "big")
            cn = (c_hi << 16) | (c_mid << 8) | c_lo
            resp.parsed = (raw, internal_id, fc, cn, phase, offset, tries)
        return resp

    @expect_response(Status.LF_TAG_OK)
    def keri_write_to_t55xx(self, frame8: bytes, new_key: bytes = None,
                            old_keys: list = None):
        """Write a raw 64-bit Keri frame onto a T55xx tag (PSK1, RF/32).

        ⛔ `frame8` is the AIR frame — E0000000 followed by the internal id — NOT the block
        contents. The firmware rotates it into the form a T5577 clocks out; the two differ
        by three bits and transcribing one as the other yields a tag nothing reads.

        ⚠ Returns LF_TAG_OK regardless — a T5577 does not acknowledge a write.
        """
        if len(frame8) != 8:
            raise ValueError("The raw frame must be exactly 8 bytes")
        new_key = new_key if new_key is not None else globals()["new_key"]
        old_keys = old_keys or globals()["old_keys"]
        data = struct.pack(f'!8s4s{4*len(old_keys)}s', frame8, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.KERI_WRITE_TO_T55XX, data)

    @expect_response(Status.SUCCESS)
    def keri_set_emu_id(self, id: bytes):
        """Set the 64-bit Keri frame emulated on the active slot.

        :param id: 8 bytes, MSB first on air. The first 33 bits are Keri's preamble, so a
                   valid frame always starts E0000000 and the next byte has its top bit set.
        """
        if len(id) != 8:
            raise ValueError("The id bytes length must equal 8")
        return self.device.send_cmd_sync(Command.KERI_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def keri_get_emu_id(self):
        """Get the emulated Keri 64-bit frame."""
        resp = self.device.send_cmd_sync(Command.KERI_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[:8]
        return resp

    @expect_response(Status.LF_TAG_OK)
    def fdxa_scan(self):
        """Read an FDX-A credential (FSK2a carrying Manchester, 96-bit frame).

        ⭐ The most heavily gated format here: a 16-bit preamble, the frame repeating, 40
        Manchester pair checks, and odd parity on each of the five decoded bytes.

        ⚠ Bit 7 of every decoded byte IS that parity bit, so an arbitrary 5-byte string is
        not a valid FDX-A credential (C199).
        """
        resp = self.device.send_cmd_sync(Command.FDXA_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            raw, pay, phase, tries = struct.unpack(">12s5sBB1x", resp.data[:20])
            resp.parsed = (raw, pay, phase, tries)
        return resp

    @expect_response(Status.LF_TAG_OK)
    def paradox_scan(self):
        """Read a Paradox credential (FSK2a, 96-bit frame).

        ⭐ Its gate is structural rather than a checksum: an 8-bit preamble, the frame
        repeating, and 44 INDEPENDENT alternating-pair checks over bits 8..95.
        """
        resp = self.device.send_cmd_sync(Command.PARADOX_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            raw, phase, tries = struct.unpack(">12sBB2x", resp.data[:16])
            resp.parsed = (raw, phase, tries)
        return resp

    @expect_response(Status.LF_TAG_OK)
    def pyramid_scan(self):
        """Read a Pyramid credential (FSK2a, 128-bit frame).

        ⭐ The strongest gate in the FSK family: a 24-bit preamble, the frame repeating at
        128, AND a CRC-8 over 13 bytes — poly 0x31 with both ends reflected, verified against
        a real capture rather than transcribed and hoped for.
        """
        resp = self.device.send_cmd_sync(Command.PYRAMID_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            raw, phase, tries = struct.unpack(">16sBB2x", resp.data[:20])
            resp.parsed = (raw, phase, tries)
        return resp

    @expect_response(Status.LF_TAG_OK)
    def awid_scan(self):
        """Read an AWID credential (FSK2a, RF/8 and RF/10 tones, 96-bit frame).

        ⭐ The first FSK protocol here, and it goes through the SAME capture engine as the PSK
        and ASK families — NOT the HID/ioProx per-protocol SAADC reader where HID's
        unexplained intermittency lives (C193).

        ⚠ AWID carries only 66 payload bits, so the last 6 bits of the 9-byte credential are
        not on the wire and come back zero. That is the format, not a truncation here.

        Returns (raw12, payload9, phase, tries).
        """
        resp = self.device.send_cmd_sync(Command.AWID_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            raw, pay, phase, tries = struct.unpack(">12s9sBB1x", resp.data[:24])
            resp.parsed = (raw, pay, phase, tries)
        return resp

    @expect_response(Status.LF_TAG_OK)
    def fdxb_scan(self):
        """Read an FDX-B credential (ISO 11784/11785, ASK biphase inverted, RF/32, 128 bits).

        ⚠ NOT FDX-A, which is FSK2a and a different protocol entirely — the Proxmark's
        `lf fdx` is this one, which is why FDX-A has no writer anywhere (C185, C201).

        Returns (raw16, phase, bit_pos, tries, inverted).
        """
        resp = self.device.send_cmd_sync(Command.FDXB_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            raw, phase, pos, tries, inv = struct.unpack(">16sBBBB", resp.data[:20])
            resp.parsed = (raw, phase, pos, tries, bool(inv))
        return resp

    @expect_response(Status.LF_TAG_OK)
    def fdxa_write_to_t55xx(self, frame12: bytes, new_key: bytes = None,
                            old_keys: list = None):
        """Write a raw 96-bit FDX-A frame onto a T55xx (FSK2, RF/50, 3 data blocks).

        ⛔ FSK2, not the FSK2a every other FSK protocol here uses — one bit apart in the config
        word. Measured off `lf destron clone` (C339). ⚠ Pass the frame exactly as
        `lf fdxa read` prints it; the device complements it, because an FSK2 tag stores the
        inverse of what an FSK2a-path reader reports.
        """
        if len(frame12) != 12:
            raise ValueError("The raw frame must be exactly 12 bytes")
        new_key = new_key if new_key is not None else globals()["new_key"]
        old_keys = old_keys or globals()["old_keys"]
        data = struct.pack(f'!12s4s{4*len(old_keys)}s', frame12, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.FDXA_WRITE_TO_T55XX, data)

    def fdxb_write_to_t55xx(self, frame16: bytes, new_key: bytes = None,
                            old_keys: list = None):
        """Write a raw 128-bit FDX-B frame onto a T55xx (DIPHASE, RF/32, 4 data blocks).

        ⚠ DIPHASE, not GProxII's BIPHASE — one bit apart in the config word, and the wrong one
        writes a tag nothing here can read. Measured from the reference clone's dump (C214).
        """
        if len(frame16) != 16:
            raise ValueError("The raw frame must be exactly 16 bytes")
        new_key = new_key if new_key is not None else globals()["new_key"]
        old_keys = old_keys or globals()["old_keys"]
        data = struct.pack(f'!16s4s{4*len(old_keys)}s', frame16, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.FDXB_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def gproxii_scan(self):
        """Read a GProxII credential (ASK BIPHASE, RF/64, 96-bit frame).

        ⭐ The fourth decode path on this device, and the first that never asks what level the
        envelope is sitting at — the front end's AC coupling has decayed away inside an RF/64
        half-bit, so the decoder asks only whether the middle of each bit STEPPED (C204).

        Returns (raw12, phase, bit_pos, tries, inverted).
        """
        resp = self.device.send_cmd_sync(Command.GPROXII_SCAN, timeout=20)
        if resp.status == Status.LF_TAG_OK:
            raw, phase, pos, tries, inv = struct.unpack(">12sBBBB", resp.data[:16])
            resp.parsed = (raw, phase, pos, tries, bool(inv))
        elif len(resp.data) == 4:
            # ⚠ Instrumentation: on failure the firmware returns the decoder's energy, which
            # says whether the CAPTURE was wrong or the DECODE was. See the firmware note.
            resp.parsed = ("energy", struct.unpack(">i", resp.data)[0])
        return resp

    @expect_response(Status.LF_TAG_OK)
    def gproxii_write_to_t55xx(self, frame12: bytes, new_key: bytes = None,
                               old_keys: list = None):
        """Write a raw 96-bit GProxII frame onto a T55xx (BIPHASE, RF/64, 3 data blocks).

        ⭐ `frame12` is BOTH the air frame and the block contents — measured off the reference
        clone's own dump (C204), the question Keri answers differently (C160).
        """
        if len(frame12) != 12:
            raise ValueError("The raw frame must be exactly 12 bytes")
        new_key = new_key if new_key is not None else globals()["new_key"]
        old_keys = old_keys or globals()["old_keys"]
        data = struct.pack(f'!12s4s{4*len(old_keys)}s', frame12, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.GPROXII_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def awid_write_to_t55xx(self, frame12: bytes, new_key: bytes = None,
                            old_keys: list = None):
        """Write a raw 96-bit AWID frame onto a T55xx (FSK2a, RF/50, 3 data blocks).

        ⭐ `frame12` is BOTH the air frame and the block contents. That was MEASURED off a
        Proxmark clone's own dump, not assumed from the ASK protocols — Keri's block form is
        three bits out of phase with its air frame and emitting the wrong one of the two gave
        a stable WRONG credential 6 of 6 (C160, C202).

        ⚠ Returns LF_TAG_OK regardless — a T5577 does not acknowledge a write.
        """
        if len(frame12) != 12:
            raise ValueError("The raw frame must be exactly 12 bytes")
        new_key = new_key if new_key is not None else globals()["new_key"]
        old_keys = old_keys or globals()["old_keys"]
        data = struct.pack(f'!12s4s{4*len(old_keys)}s', frame12, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.AWID_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def paradox_write_to_t55xx(self, frame12: bytes, new_key: bytes = None,
                               old_keys: list = None):
        """Write a raw 96-bit Paradox frame onto a T55xx (FSK2a, RF/50, 3 data blocks).

        ⚠ The same config word as AWID's, exactly — the two differ only in preamble and
        payload layout, which the writer never sees (C202).
        """
        if len(frame12) != 12:
            raise ValueError("The raw frame must be exactly 12 bytes")
        new_key = new_key if new_key is not None else globals()["new_key"]
        old_keys = old_keys or globals()["old_keys"]
        data = struct.pack(f'!12s4s{4*len(old_keys)}s', frame12, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.PARADOX_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def pyramid_write_to_t55xx(self, frame16: bytes, new_key: bytes = None,
                               old_keys: list = None):
        """Write a raw 128-bit Pyramid frame onto a T55xx (FSK2a, RF/50, 4 data blocks).

        ⚠ SIXTEEN bytes, not twelve, and four data blocks rather than three — that block count
        is the only part of the config word Pyramid does not share with AWID and Paradox.
        """
        if len(frame16) != 16:
            raise ValueError("The raw frame must be exactly 16 bytes")
        new_key = new_key if new_key is not None else globals()["new_key"]
        old_keys = old_keys or globals()["old_keys"]
        data = struct.pack(f'!16s4s{4*len(old_keys)}s', frame16, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.PYRAMID_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def instafob_scan(self):
        """Read an InstaFob credential (ASK/Manchester, RF/32, 225-bit frame).

        ⭐ The frame is the tag's ENTIRE T5577 page 0 — 1 + 7x32 bits — and the 32 bits this
        format is recognised by are the chip's own configuration word `0x00107060`.

        ⚠ THE FRAME IS A 7-BIT ROTATION of Momentum's numbering: we search for the config word
        and return 225 bits from there, where Momentum puts it at bit 7 (C186).

        ⛔ There is no `instafob_write_to_t55xx`, deliberately — nothing on this bench can
        verify such a write, so it would ship on self-certification (C185).

        Returns (raw29, phase, offset, tries).
        """
        resp = self.device.send_cmd_sync(Command.INSTAFOB_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            raw, phase, offset, tries = struct.unpack(">29sBBB", resp.data[:32])
            resp.parsed = (raw, phase, offset, tries)
        return resp

    @expect_response(Status.LF_TAG_OK)
    def noralsy_scan(self):
        """Read a Noralsy credential (ASK/Manchester, RF/32, 96-bit frame).

        ⭐ The strongest gate in the ASK family: a 12-bit preamble plus TWO computed 4-bit
        checksums. Also the fastest read — 6144 samples (49ms) against Gallagher's 14336.
        """
        resp = self.device.send_cmd_sync(Command.NORALSY_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            raw, phase, offset, tries = struct.unpack(">12sBBB1x", resp.data[:16])
            resp.parsed = (raw, phase, offset, tries)
        return resp

    @expect_response(Status.LF_TAG_OK)
    def noralsy_write_to_t55xx(self, frame12: bytes, new_key: bytes = None,
                               old_keys: list = None):
        """Write a raw 96-bit Noralsy frame onto a T55xx (ASK, RF/32, 3 data blocks)."""
        if len(frame12) != 12:
            raise ValueError("The raw frame must be exactly 12 bytes")
        new_key = new_key if new_key is not None else globals()["new_key"]
        old_keys = old_keys or globals()["old_keys"]
        data = struct.pack(f'!12s4s{4*len(old_keys)}s', frame12, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.NORALSY_WRITE_TO_T55XX, data)

    @expect_response(Status.SUCCESS)
    def noralsy_set_emu_id(self, id: bytes):
        """Set the 96-bit Noralsy frame emulated on the active slot."""
        if len(id) != 12:
            raise ValueError("The id bytes length must equal 12")
        return self.device.send_cmd_sync(Command.NORALSY_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def noralsy_get_emu_id(self):
        """Get the emulated Noralsy 96-bit frame."""
        resp = self.device.send_cmd_sync(Command.NORALSY_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[:12]
        return resp

    @expect_response(Status.LF_TAG_OK)
    def securakey_scan(self):
        """Read a Securakey credential (ASK/Manchester, RF/40, 96-bit frame).

        ⚠ Its only gate is a 19-bit preamble — no checksum is known to either reference
        implementation — so this format is less strongly verified than Gallagher's (C175).

        Returns (raw, phase, offset, tries).
        """
        resp = self.device.send_cmd_sync(Command.SECURAKEY_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            raw, phase, offset, tries = struct.unpack(">12sBBB1x", resp.data[:16])
            resp.parsed = (raw, phase, offset, tries)
        return resp

    @expect_response(Status.LF_TAG_OK)
    def securakey_write_to_t55xx(self, frame12: bytes, new_key: bytes = None,
                                 old_keys: list = None):
        """Write a raw 96-bit Securakey frame onto a T55xx (ASK, RF/40, 3 data blocks)."""
        if len(frame12) != 12:
            raise ValueError("The raw frame must be exactly 12 bytes")
        new_key = new_key if new_key is not None else globals()["new_key"]
        old_keys = old_keys or globals()["old_keys"]
        data = struct.pack(f'!12s4s{4*len(old_keys)}s', frame12, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.SECURAKEY_WRITE_TO_T55XX, data)

    @expect_response(Status.SUCCESS)
    def securakey_set_emu_id(self, id: bytes):
        """Set the 96-bit Securakey frame emulated on the active slot."""
        if len(id) != 12:
            raise ValueError("The id bytes length must equal 12")
        return self.device.send_cmd_sync(Command.SECURAKEY_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def securakey_get_emu_id(self):
        """Get the emulated Securakey 96-bit frame."""
        resp = self.device.send_cmd_sync(Command.SECURAKEY_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[:12]
        return resp

    @expect_response(Status.SUCCESS)
    def gproxii_set_emu_id(self, id: bytes):
        """Set the 96-bit GProxII frame emulated on the active slot.

        ⚠ Biphase: the emitter holds ONE level per half-bit, two entries per bit, at
        `counter_top` 32 — the magnitude the working ASK emitters use, against AWID's 8 and 10
        (C221).

        :param id: 12 bytes, MSB first on air, preamble included.
        """
        if len(id) != 12:
            raise ValueError("The id bytes length must equal 12")
        return self.device.send_cmd_sync(Command.GPROXII_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def gproxii_get_emu_id(self):
        """Get the emulated GProxII 96-bit frame."""
        resp = self.device.send_cmd_sync(Command.GPROXII_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data
        return resp

    @expect_response(Status.SUCCESS)
    def awid_set_emu_id(self, id: bytes):
        """Set the 96-bit AWID frame emulated on the active slot.

        ⚠ FSK2a, so the emitter spends one PWM entry per TONE PERIOD rather than per bit —
        six for a 0 and five for a 1 — and the sequence length therefore depends on the data
        (C216). Nothing here needs to know that, but a frame length that changes with content
        is worth knowing about when reading the firmware.

        :param id: 12 bytes, MSB first on air, preamble included.
        """
        if len(id) != 12:
            raise ValueError("The id bytes length must equal 12")
        return self.device.send_cmd_sync(Command.AWID_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def awid_get_emu_id(self):
        """Get the emulated AWID 96-bit frame."""
        resp = self.device.send_cmd_sync(Command.AWID_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data
        return resp

    @expect_response(Status.SUCCESS)
    def gallagher_set_emu_id(self, id: bytes):
        """Set the 96-bit Gallagher frame emulated on the active slot.

        :param id: 12 bytes, MSB first on air. A valid frame starts 0x7FEA.
        """
        if len(id) != 12:
            raise ValueError("The id bytes length must equal 12")
        return self.device.send_cmd_sync(Command.GALLAGHER_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def gallagher_get_emu_id(self):
        """Get the emulated Gallagher 96-bit frame."""
        resp = self.device.send_cmd_sync(Command.GALLAGHER_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[:12]
        return resp

    @expect_response(Status.LF_TAG_OK)
    def gallagher_scan(self):
        """
        Read a Gallagher credential (ASK/Manchester, RF/32, 96-bit frame).

        ⭐ The first protocol of the biphase family, and it goes through the SAME capture
        engine as the PSK readers — only the decoder differs. Returns (raw, phase, offset,
        tries); the region / facility / card / issue are descrambled by the CLI, not here.

        ⚠ Slower than the PSK reads: the capture is 14336 samples (114ms) against 4096 (33ms)
        for Indala26, because the threshold was measured at 10240 (C172).
        """
        resp = self.device.send_cmd_sync(Command.GALLAGHER_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            raw, phase, offset, tries = struct.unpack(">12sBBB1x", resp.data[:16])
            resp.parsed = (raw, phase, offset, tries)
        return resp

    @expect_response(Status.LF_TAG_OK)
    def gallagher_write_to_t55xx(self, frame12: bytes, new_key: bytes = None,
                                 old_keys: list = None):
        """Write a raw 96-bit Gallagher frame onto a T55xx (ASK, RF/32, 3 data blocks).

        ⭐ `frame12` is BOTH the air frame and the block contents — Gallagher's frame starts at
        a block boundary, so the firmware transcribes rather than rotating (C158, C160).

        ⚠ Returns LF_TAG_OK regardless — a T5577 does not acknowledge a write.
        """
        if len(frame12) != 12:
            raise ValueError("The raw frame must be exactly 12 bytes")
        new_key = new_key if new_key is not None else globals()["new_key"]
        old_keys = old_keys or globals()["old_keys"]
        data = struct.pack(f'!12s4s{4*len(old_keys)}s', frame12, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.GALLAGHER_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def nexwatch_scan(self):
        """
        Read a NexWatch credential (PSK1, RF/32, fc/2 subcarrier, 96-bit frame).

        ⭐ The same capture engine and demodulator as Indala, IDTECK and Keri — the fourth
        protocol through `lf_psk1_format_t`. Only the preamble, the acceptance rule and the
        payload interpretation differ.

        Returns (raw, cn, magic, mode, phase, offset) where raw is the full 96-bit frame.

        ⚠ `magic` is INFERRED, not read: it is not carried in the frame at all. The firmware
        tries the three known vendor constants (0xBE Quadrakey, 0x88 Nexkey, 0x86 Honeywell)
        and reports whichever reproduces the frame's checksum, or 0x00 for none — which is a
        valid read from an unknown vendor, not a failure.
        """
        resp = self.device.send_cmd_sync(Command.NEXWATCH_SCAN, timeout=10)
        if resp.status == Status.LF_TAG_OK:
            raw, cn, magic, mode, phase, offset = struct.unpack(">12sIBBBB", resp.data[:20])
            resp.parsed = (raw, cn, magic, mode, phase, offset)
        return resp

    @expect_response(Status.LF_TAG_OK)
    def nexwatch_write_to_t55xx(self, frame12: bytes, new_key: bytes = None,
                                old_keys: list = None):
        """Write a raw 96-bit NexWatch frame onto a T55xx tag (PSK1, RF/32, 3 data blocks).

        ⭐ `frame12` is BOTH the air frame and the block contents — NexWatch's frame starts at
        a block boundary, so the firmware transcribes rather than rotating. ⛔ That is the
        opposite of Keri, whose T5577 holds `(id << 3) | 7`; the two cases are distinguished
        by a real clone's block dump, never by assumption (C158, C160).

        ⚠ Returns LF_TAG_OK regardless — a T5577 does not acknowledge a write.
        """
        if len(frame12) != 12:
            raise ValueError("The raw frame must be exactly 12 bytes")
        new_key = new_key if new_key is not None else globals()["new_key"]
        old_keys = old_keys or globals()["old_keys"]
        data = struct.pack(f'!12s4s{4*len(old_keys)}s', frame12, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.NEXWATCH_WRITE_TO_T55XX, data)

    @expect_response(Status.SUCCESS)
    def nexwatch_set_emu_id(self, id: bytes):
        """Set the 96-bit NexWatch frame emulated on the active slot.

        :param id: 12 bytes, MSB first on air. A valid frame starts 0x56 followed by four
                   zero bytes — the 8-bit magic and the 32 reserved bits.
        """
        if len(id) != 12:
            raise ValueError("The id bytes length must equal 12")
        return self.device.send_cmd_sync(Command.NEXWATCH_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def nexwatch_get_emu_id(self):
        """Get the emulated NexWatch 96-bit frame."""
        resp = self.device.send_cmd_sync(Command.NEXWATCH_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[:12]
        return resp

    @expect_response(Status.LF_TAG_OK)
    def ioprox_write_to_t55xx(self, id_bytes: bytes):
        """
        Write ioProx card data to a T55XX tag.
        """
        if len(id_bytes) != 16:
            raise ValueError("The ioProx id bytes length must equal 16")

        # Pack id_bytes (16), new_key (4), and all old_keys (4 each) into one buffer
        fmt = f'!16s4s{4 * len(old_keys)}s'
        data = struct.pack(fmt, id_bytes, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.IOPROX_WRITE_TO_T55XX, data)

    @expect_response(Status.SUCCESS)
    def ioprox_decode_raw(self, raw8_bytes):
        """
        Send 8 raw card bytes to firmware and return 16-byte card data structure.
        Response layout: [0]=ver, [1]=fc, [2..3]=cn, [4..11]=raw8, [12..15]=padding.
        """
        resp = self.device.send_cmd_sync(Command.IOPROX_DECODE_RAW, data=raw8_bytes)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack(">BBH8sBBBB", resp.data[:16])
        return resp

    @expect_response(Status.SUCCESS)
    def ioprox_compose_id(self, ver, fc, cn):
        """
        Encode ioProx parameters into a 16-byte card data structure via firmware.
        Response layout: [0]=ver, [1]=fc, [2..3]=cn, [4..11]=raw8, [12..15]=padding.
        """
        payload = struct.pack(">BBH", ver, fc, cn)
        resp = self.device.send_cmd_sync(Command.IOPROX_COMPOSE_ID, data=payload)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack(">BBH8sBBBB", resp.data[:16])
        return resp

    def lf_sniff(self, timeout_ms: int = 2000, bits: int = 8, phase: int = 0,
                 rate_khz: int = 0, gain: int = 6, settle_ms: int = 0,
                 input_ain: int = 5, drive: int = 4):
        """
        Capture raw LF field ADC samples.

        The ChameleonUltra samples the LF antenna at 125kHz (8µs/sample).
        Each byte is an 8-bit ADC value: ~0x80 = field on, lower = gap/no field.

        ⚠ 8-bit mode discards 5 bits of every conversion. The SAADC runs at 14-bit and
        the protocol decoders get that value intact; only this debug path truncates, which
        makes a weak subcarrier look absent when it is merely sub-LSB. bits=16 returns the
        full conversion as 2 bytes/sample big-endian — at the cost of half the duration,
        since the 4000-byte frame limit is on BYTES, not samples.

        :param timeout_ms: Capture duration in ms (1-10000, default 2000)
        :param bits: 8 (default, historical format) or 16 (full 14-bit, big-endian)
        :param rate_khz: 0 samples once per carrier period (125kHz, locked to the field).
            Otherwise free-runs the sample trigger at this rate, ASYNCHRONOUS to the
            carrier, so an fc/2 subcarrier is oversampled instead of sitting at Nyquist.
            200 is the practical ceiling (SAADC conversion time).
        :param phase: SAADC sample phase, 0-127 ticks of 62.5ns after the carrier period
            boundary. 0 keeps the default trigger. 128 ticks span one carrier period, so
            this walks 0-360° of the carrier and 0-720° of an fc/2 subcarrier.
        :param input_ain: SAADC input node. 5 = AIN5 / LF_OA_OUT, the stock tap after both
            RC filter poles. 0 = AIN0 / LF_RSSI, which taps LF_OA upstream of both — worth
            ~9.7dB at fc/2 if the noise is made downstream, but fed through 470k, which is
            far above what the converter's 5us acquisition can settle. See ble_main.h.
        :param drive: Reader field strength as the PWM mark against a top_value of 4:
            1..3, stock 2 (50% duty). ⭐ Weakening our own field is the only way to reduce
            the signal reaching the LF amplifier WITHOUT moving the tag, and on a strongly
            coupled tag that amplifier saturates — a third of a PAC capture pins at a rail,
            upstream of the ADC where no gain setting reaches it (C140). The fundamental
            scales as sin(pi*D), so 1 or 3 is about -3dB against 2.
        :return: Raw response object — check .status and .data
        """
        if drive not in range(1, 8):
            raise ValueError("drive must be 1..7")
        timeout_ms = max(1, min(10000, timeout_ms))
        if bits not in (8, 16):
            raise ValueError("bits must be 8 or 16")
        if not 0 <= phase <= 127:
            raise ValueError("phase must be 0..127 ticks")
        if rate_khz and not 10 <= rate_khz <= 200:
            raise ValueError("rate_khz must be 0 (carrier-locked) or 10..200")
        if gain not in (0, 1, 2, 3, 4, 5, 6):
            raise ValueError("gain must be the divisor 1..6 (6 = stock 1/6)")
        if not 0 <= settle_ms <= 255:
            raise ValueError("settle_ms must be 0..255 (0 = the historical 2ms)")
        if input_ain not in (0, 5):
            raise ValueError("input_ain must be 5 (AIN5/LF_OA_OUT) or 0 (AIN0/LF_RSSI)")
        timeout_s = (timeout_ms // 1000) + 2

        # ⭐ Reassemble the chunked response. Chunk 0 performs the capture; later chunks
        # slice the SAME acquisition on-device, so this is one contiguous capture and not
        # several stitched together. A short or empty chunk means the end.
        first = None
        buf = bytearray()
        for chunk in range(16):
            payload = bytes([(timeout_ms >> 8) & 0xFF, timeout_ms & 0xFF, bits, phase,
                             rate_khz, gain, settle_ms, chunk, input_ain, drive])
            resp = self.device.send_cmd_sync(Command.LF_SNIFF, payload, timeout=timeout_s)
            if first is None:
                first = resp
                if resp.status != Status.LF_TAG_OK or not resp.data:
                    return resp
            elif resp.status != Status.LF_TAG_OK or not resp.data:
                break
            buf += bytes(resp.data)
            if len(resp.data) < LF_SNIFF_CHUNK_BYTES:
                break                     # a short chunk is the last one
        first.data = bytes(buf)
        return first

    @expect_response(Status.LF_TAG_OK)
    def em4x05_scan(self, pwd: int = 0):
        """
        Read an EM4x05 or EM4x69 tag (reader-talk-first).

        Response payload (14 bytes, big-endian):
          config    4 bytes  — block 0 configuration word
          uid       4 bytes  — EM4x05 UID
          uid_hi    4 bytes  — EM4x69 uid_hi (zero for plain EM4x05)
          is_em4x69 1 byte   — 1 if a 64-bit EM4x69 UID was read
          uid_block 1 byte   — block number UID was read from

        :param pwd: 32-bit password for LOGIN (default 0x00000000)
        :return: parsed tuple (config, uid, uid_hi, is_em4x69, uid_block)
        """
        pwd_bytes = struct.pack('!I', pwd & 0xFFFFFFFF)
        resp = self.device.send_cmd_sync(Command.EM4X05_SCAN, pwd_bytes)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = struct.unpack('!IIIBB', resp.data[:14])
        return resp

    @expect_response(Status.LF_TAG_OK)
    def viking_scan(self):
        """
        Read the card number of Viking.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.VIKING_SCAN)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = resp.data  # uid
        return resp

    @expect_response(Status.LF_TAG_OK)
    def viking_write_to_t55xx(self, id_bytes: bytes):
        """
        Write Viking card number into T55XX.

        :param id_bytes: ID card number
        :return:
        """
        if len(id_bytes) != 4:
            raise ValueError("The id bytes length must equal 4")
        data = struct.pack(f'!4s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.VIKING_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def pac_scan(self):
        """
        Read the card ID of PAC/Stanley.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.PAC_SCAN)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = resp.data[:8]
        return resp

    @expect_response(Status.LF_TAG_OK)
    def pac_write_to_t55xx(self, id_bytes: bytes):
        """
        Write PAC/Stanley card data to a T55XX tag.

        :param id_bytes: 8-byte ASCII card ID
        :return:
        """
        if len(id_bytes) != 8:
            raise ValueError("The id bytes length must equal 8")
        data = struct.pack(f'!8s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.PAC_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def jablotron_scan(self):
        """
        Read the card number of Jablotron.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.JABLOTRON_SCAN)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = resp.data[:5]
        return resp

    @expect_response(Status.LF_TAG_OK)
    def jablotron_write_to_t55xx(self, id_bytes: bytes):
        """
        Write Jablotron card number into T55XX.

        :param id_bytes: 5-byte Jablotron card ID
        :return:
        """
        if len(id_bytes) != 5:
            raise ValueError("The id bytes length must equal 5")
        data = struct.pack(f'!5s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.JABLOTRON_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def idteck_write_to_t55xx(self, id_bytes: bytes):
        """
        Write an IDTECK 64-bit PSK1 frame onto a T55xx tag.

        :param id_bytes: 8 bytes = full 64-bit frame (preamble 4 bytes + data 4 bytes)
        :return:
        """
        if len(id_bytes) != 8:
            raise ValueError("The id bytes length must equal 8")
        data = struct.pack(f'!8s4s{4*len(old_keys)}s', id_bytes, new_key, b''.join(old_keys))
        return self.device.send_cmd_sync(Command.IDTECK_WRITE_TO_T55XX, data)

    @expect_response(Status.LF_TAG_OK)
    def adc_generic_read(self):
        """
        Read the ADC when the field is on.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.ADC_GENERIC_READ, None)
        if resp.status == Status.LF_TAG_OK:
            resp.parsed = resp.data
        return resp

    @expect_response(Status.SUCCESS)
    def get_slot_info(self):
        """
        Get slots info.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.GET_SLOT_INFO)
        if resp.status == Status.SUCCESS:
            resp.parsed = [{'hf': hf, 'lf': lf}
                           for hf, lf in struct.iter_unpack('!HH', resp.data)]
        return resp

    @expect_response(Status.SUCCESS)
    def get_active_slot(self):
        """
        Get selected slot.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.GET_ACTIVE_SLOT)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def set_active_slot(self, slot_index: SlotNumber):
        """
        Set the card slot currently active for use.

        :param slot_index: Card slot index
        :return:
        """
        # SlotNumber() will raise error for us if slot_index not in slot range
        data = struct.pack('!B', SlotNumber.to_fw(slot_index))
        return self.device.send_cmd_sync(Command.SET_ACTIVE_SLOT, data)

    @expect_response(Status.SUCCESS)
    def set_slot_tag_type(self, slot_index: SlotNumber, tag_type: TagSpecificType):
        """
        Set the label type of the emulated card of the current card slot
        Note: This operation will not change the data in the flash,
        and the change of the data in the flash will only be updated at the next save.

        :param slot_index:  Card slot number
        :param tag_type:  label type
        :return:
        """
        # SlotNumber() will raise error for us if slot_index not in slot range
        data = struct.pack('!BH', SlotNumber.to_fw(slot_index), tag_type)
        return self.device.send_cmd_sync(Command.SET_SLOT_TAG_TYPE, data)

    @expect_response(Status.SUCCESS)
    def delete_slot_sense_type(self, slot_index: SlotNumber, sense_type: TagSenseType):
        """
        Delete a sense type for a specific slot.

        :param slot_index: Slot index
        :param sense_type: Sense type to disable
        :return:
        """
        data = struct.pack('!BB', SlotNumber.to_fw(slot_index), sense_type)
        return self.device.send_cmd_sync(Command.DELETE_SLOT_SENSE_TYPE, data)

    @expect_response(Status.SUCCESS)
    def set_slot_data_default(self, slot_index: SlotNumber, tag_type: TagSpecificType):
        """
        Set the data of the emulated card in the specified card slot as the default data
        Note: This API will set the data in the flash together.

        :param slot_index: Card slot number
        :param tag_type:  The default label type to set
        :return:
        """
        # SlotNumber() will raise error for us if slot_index not in slot range
        data = struct.pack('!BH', SlotNumber.to_fw(slot_index), tag_type)
        return self.device.send_cmd_sync(Command.SET_SLOT_DATA_DEFAULT, data)

    @expect_response(Status.SUCCESS)
    def set_slot_enable(self, slot_index: SlotNumber, sense_type: TagSenseType, enabled: bool):
        """
        Set whether the specified card slot is enabled.

        :param slot_index: Card slot number
        :param enable: Whether to enable
        :return:
        """
        # SlotNumber() will raise error for us if slot_index not in slot range
        data = struct.pack('!BBB', SlotNumber.to_fw(slot_index), sense_type, enabled)
        return self.device.send_cmd_sync(Command.SET_SLOT_ENABLE, data)

    def _get_active_lf_tag_type(self) -> TagSpecificType:
        slotinfo = self.get_slot_info()
        active_slot = SlotNumber.from_fw(self.get_active_slot())
        lf_tag_value = slotinfo[active_slot - 1]['lf']
        return TagSpecificType(lf_tag_value)

    @expect_response(Status.SUCCESS)
    def em410x_set_emu_id(self, id: bytes):
        """
        Set the card number emulated by EM410x.

        :param id_bytes: byte of the card number
        :return:
        """
        lf_tag_type = self._get_active_lf_tag_type()
        if lf_tag_type == TagSpecificType.EM410X_ELECTRA:
            expected_len = 13
        elif lf_tag_type == TagSpecificType.EM410X:
            expected_len = 5
        else:
            raise ValueError(f"Active LF slot type {lf_tag_type} is not EM410X")

        if len(id) != expected_len:
            raise ValueError(f"The id bytes length must equal {expected_len}")

        data = struct.pack(f'!{expected_len}s', id)
        return self.device.send_cmd_sync(Command.EM410X_SET_EMU_ID, data)

    @expect_response(Status.SUCCESS)
    def em410x_get_emu_id(self):
        """
        Get the emulated EM410x card id
        """
        resp = self.device.send_cmd_sync(Command.EM410X_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            data = resp.data
            id_bytes = data
            tag_type = None

            if len(data) >= 2:
                try:
                    candidate = TagSpecificType(int.from_bytes(data[:2], byteorder='big'))
                except ValueError:
                    candidate = None

                if candidate in (TagSpecificType.EM410X, TagSpecificType.EM410X_ELECTRA):
                    expected_len = 13 if candidate == TagSpecificType.EM410X_ELECTRA else 5
                    if len(data) == expected_len + 2:
                        tag_type = candidate
                        id_bytes = data[2:2 + expected_len]

            if tag_type is None:
                lf_tag_type = self._get_active_lf_tag_type()
                if lf_tag_type == TagSpecificType.EM410X_ELECTRA:
                    expected_len = 13
                elif lf_tag_type == TagSpecificType.EM410X:
                    expected_len = 5
                else:
                    expected_len = len(data)
                id_bytes = data[:expected_len]

            resp.parsed = id_bytes
        return resp

    @expect_response(Status.SUCCESS)
    def hidprox_set_emu_id(self, id: bytes):
        """
        Set the card number emulated by HID Prox.

        :param id_bytes: byte of the card number
        :return:
        """
        if len(id) != 13:
            raise ValueError("The id bytes length must equal 13")
        return self.device.send_cmd_sync(Command.HIDPROX_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def hidprox_get_emu_id(self):
        """
        Get the emulated HID Prox card id
        """
        resp = self.device.send_cmd_sync(Command.HIDPROX_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('>BIBIBH', resp.data[:13])
        return resp

    @expect_response(Status.SUCCESS)
    def ioprox_set_emu_id(self, id: bytes):
        """
        Set the card number emulated by ioProx.

        :param id_bytes: byte of the card number
        :return:
        """
        if len(id) != 16:
            raise ValueError("The id bytes length must equal 16")
        return self.device.send_cmd_sync(Command.IOPROX_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def ioprox_get_emu_id(self):
        """
        Get the emulated ioProx card id
        """
        resp = self.device.send_cmd_sync(Command.IOPROX_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack(">BBH8sBBBB", resp.data[:16])
        return resp

    @expect_response(Status.SUCCESS)
    def idteck_set_emu_id(self, id: bytes):
        """
        Set the 64-bit IDTECK frame emulated on the active slot.

        :param id: 8 bytes (preamble + card data, big-endian)
        """
        if len(id) != 8:
            raise ValueError("The id bytes length must equal 8")
        return self.device.send_cmd_sync(Command.IDTECK_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def idteck_get_emu_id(self):
        """
        Get the emulated IDTECK 64-bit frame.
        """
        resp = self.device.send_cmd_sync(Command.IDTECK_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[:8]
        return resp

    @expect_response(Status.SUCCESS)
    def indala_set_emu_id(self, id: bytes):
        """
        Set the 64-bit Indala frame emulated on the active slot.

        :param id: 8 bytes, MSB first on air. The first 33 bits are Indala's fixed
                   preamble (1010, 28 zeros, 1), which is why a valid frame always
                   starts a0000000 and the next byte has its top bit set.
        """
        if len(id) != 8:
            raise ValueError("The id bytes length must equal 8")
        return self.device.send_cmd_sync(Command.INDALA_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def indala_get_emu_id(self):
        """
        Get the emulated Indala 64-bit frame.
        """
        resp = self.device.send_cmd_sync(Command.INDALA_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[:8]
        return resp

    @expect_response(Status.SUCCESS)
    def indala224_set_emu_id(self, id: bytes):
        """
        Set the 224-bit Indala frame emulated on the active slot.

        :param id: 28 bytes, MSB first on air. The first 30 bits are the Indala224
                   preamble — a 1 followed by 29 zeros — so a valid frame starts 8000000
                   and the eighth hex digit is 0 or 1.

        ⛔ Transmitted PSK2, not PSK1. See utils/psk1.h and C99.
        """
        if len(id) != 28:
            raise ValueError("The id bytes length must equal 28")
        return self.device.send_cmd_sync(Command.INDALA224_SET_EMU_ID, id)

    @expect_response(Status.SUCCESS)
    def indala224_get_emu_id(self):
        """
        Get the emulated Indala 224-bit frame.
        """
        resp = self.device.send_cmd_sync(Command.INDALA224_GET_EMU_ID)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[:28]
        return resp

    @expect_response(Status.SUCCESS)
    def viking_set_emu_id(self, id: bytes):
        """
        Set the card number emulated by Viking.

        :param id_bytes: byte of the card number
        :return:
        """
        if len(id) != 4:
            raise ValueError("The id bytes length must equal 4")
        data = struct.pack('4s', id)
        return self.device.send_cmd_sync(Command.VIKING_SET_EMU_ID, data)

    @expect_response(Status.SUCCESS)
    def viking_get_emu_id(self):
        """
        Get the emulated Viking card id
        """
        resp = self.device.send_cmd_sync(Command.VIKING_GET_EMU_ID)
        resp.parsed = resp.data
        return resp

    @expect_response(Status.SUCCESS)
    def pac_set_emu_id(self, id: bytes):
        """
        Set the card ID emulated by PAC/Stanley.

        :param id: 8-byte ASCII card ID
        :return:
        """
        if len(id) != 8:
            raise ValueError("The id bytes length must equal 8")
        data = struct.pack('8s', id)
        return self.device.send_cmd_sync(Command.PAC_SET_EMU_ID, data)

    @expect_response(Status.SUCCESS)
    def pac_get_emu_id(self):
        """
        Get the emulated PAC/Stanley card ID
        """
        resp = self.device.send_cmd_sync(Command.PAC_GET_EMU_ID)
        resp.parsed = resp.data[:8]
        return resp

    @expect_response(Status.SUCCESS)
    def jablotron_set_emu_id(self, id: bytes):
        """
        Set the card number emulated by Jablotron.

        :param id: 5-byte Jablotron card ID
        :return:
        """
        if len(id) != 5:
            raise ValueError("The id bytes length must equal 5")
        data = struct.pack('5s', id)
        return self.device.send_cmd_sync(Command.JABLOTRON_SET_EMU_ID, data)

    @expect_response(Status.SUCCESS)
    def jablotron_get_emu_id(self):
        """
        Get the emulated Jablotron card id
        """
        resp = self.device.send_cmd_sync(Command.JABLOTRON_GET_EMU_ID)
        resp.parsed = resp.data[:5]
        return resp

    @expect_response(Status.SUCCESS)
    def mf1_set_detection_enable(self, enabled: bool):
        """
        Set whether to enable the detection of the current card slot.

        :param enable: Whether to enable
        :return:
        """
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF1_SET_DETECTION_ENABLE, data)

    @expect_response(Status.SUCCESS)
    def mf1_get_detection_count(self):
        """
        Get the statistics of the current detection records.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.MF1_GET_DETECTION_COUNT)
        if resp.status == Status.SUCCESS:
            resp.parsed, = struct.unpack('!I', resp.data)
        return resp

    @expect_response(Status.SUCCESS)
    def mf1_get_detection_log(self, index: int):
        """
        Get detection logs from the specified index position.

        :param index: start index
        :return:
        """
        data = struct.pack('!I', index)
        resp = self.device.send_cmd_sync(Command.MF1_GET_DETECTION_LOG, data)
        if resp.status == Status.SUCCESS:
            # convert
            result_list = []
            pos = 0
            while pos < len(resp.data):
                block, bitfield, uid, nt, nr, ar = struct.unpack_from('!BB4s4s4s4s', resp.data, pos)
                result_list.append({
                    'block': block,
                    'type': ['A', 'B'][bitfield & 0x01],
                    'is_nested': bool(bitfield & 0x02),
                    'uid': uid.hex(),
                    'nt': nt.hex(),
                    'nr': nr.hex(),
                    'ar': ar.hex()
                })
                pos += struct.calcsize('!BB4s4s4s4s')
            resp.parsed = result_list
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_detection_enable(self):
        """
        Get whether NTAG password detection is enabled.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_DETECTION_ENABLE)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('!B', resp.data)[0] == 1
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_set_detection_enable(self, enabled: bool):
        """
        Set whether to enable NTAG password detection.

        :param enable: Whether to enable
        :return:
        """
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF0_NTAG_SET_DETECTION_ENABLE, data)

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_detection_count(self):
        """
        Get the statistics of the current NTAG password detection records.

        :return:
        """
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_DETECTION_COUNT)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('!I', resp.data)[0]
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_detection_log(self, index: int):
        """
        Get NTAG password detection logs from the specified index position.

        :param index: start index
        :return:
        """
        data = struct.pack('!I', index)
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_DETECTION_LOG, data)
        if resp.status == Status.SUCCESS:
            # convert - each log entry is just a 4-byte password
            result_list = []
            pos = 0
            while pos < len(resp.data):
                password = resp.data[pos:pos+4]
                result_list.append({
                    'password': password.hex()
                })
                pos += 4
            resp.parsed = result_list
        return resp

    @expect_response(Status.SUCCESS)
    def mf1_write_emu_block_data(self, block_start: int, block_data: bytes):
        """
        Set the block data of the analog card of MF1.

        :param block_start:  Start setting the location of block data, including this location
        :param block_data:  The byte buffer of the block data to be set can contain multiple block data,
                            automatically from block_start  increment
        :return:
        """
        data = struct.pack(f'!B{len(block_data)}s', block_start, block_data)
        return self.device.send_cmd_sync(Command.MF1_WRITE_EMU_BLOCK_DATA, data)

    @expect_response(Status.SUCCESS)
    def mf1_read_emu_block_data(self, block_start: int, block_count: int):
        """
            Gets data for selected block range
        """
        data = struct.pack('!BB', block_start, block_count)
        resp = self.device.send_cmd_sync(Command.MF1_READ_EMU_BLOCK_DATA, data)
        resp.parsed = resp.data
        return resp

    @expect_response(Status.SUCCESS)
    def mfu_get_emu_pages_count(self):
        """
            Gets the number of pages available in the current MF0 / NTAG slot
        """
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_PAGE_COUNT)
        resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def mfu_read_emu_page_data(self, page_start: int, page_count: int):
        """
            Gets data for selected block range
        """
        data = struct.pack('!BB', page_start, page_count)
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_READ_EMU_PAGE_DATA, data)
        resp.parsed = resp.data
        return resp

    @expect_response(Status.SUCCESS)
    def mfu_write_emu_page_data(self, page_start: int, data: bytes):
        """
            Gets data for selected block range
        """
        count = len(data) >> 2

        assert (len(data) % 4) == 0
        assert (page_start >= 0) and (count + page_start) <= 256

        data = struct.pack('!BB', page_start, count) + data
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_WRITE_EMU_PAGE_DATA, data)
        return resp

    @expect_response(Status.SUCCESS)
    def mfu_read_emu_counter_data(self, index: int) -> tuple[int, bool]:
        """
            Gets data for selected counter
        """
        data = struct.pack('!B', index)
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_COUNTER_DATA, data)
        if resp.status == Status.SUCCESS:
            resp.parsed = (((resp.data[2] << 16) | (resp.data[1] << 8) | resp.data[0]), resp.data[3] == 0xBD)
        return resp

    @expect_response(Status.SUCCESS)
    def mfu_write_emu_counter_data(self, index: int, value: int, reset_tearing: bool):
        """
            Sets data for selected counter
        """
        data = struct.pack('!BBBB', index | (int(reset_tearing) << 7),
                           value & 0xFF, (value >> 8) & 0xFF, (value >> 16) & 0xFF)
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_SET_COUNTER_DATA, data)
        return resp

    @expect_response(Status.SUCCESS)
    def mfu_reset_auth_cnt(self):
        """
            Resets authentication counter
        """
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_RESET_AUTH_CNT, bytes())
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def hf14a_set_anti_coll_data(self, uid: bytes, atqa: bytes, sak: bytes, ats: bytes = b''):
        """
        Set anti-collision data of current HF slot (UID/SAK/ATQA/ATS).

        :param uid:  uid bytes
        :param atqa: atqa bytes
        :param sak:  sak bytes
        :param ats:  ats bytes (optional)
        :return:
        """
        data = struct.pack(f'!B{len(uid)}s2s1sB{len(ats)}s', len(uid), uid, atqa, sak, len(ats), ats)
        return self.device.send_cmd_sync(Command.HF14A_SET_ANTI_COLL_DATA, data)

    @expect_response(Status.SUCCESS)
    def set_slot_tag_nick(self, slot: SlotNumber, sense_type: TagSenseType, name: str):
        """
        Set the nick name of the slot.

        :param slot:  Card slot number
        :param sense_type:  field type
        :param name:  Card slot nickname
        :return:
        """
        encoded_name = name.encode(encoding="utf8")
        if len(encoded_name) > 32:
            raise ValueError("Your tag nick name too long.")
        # SlotNumber() will raise error for us if slot not in slot range
        data = struct.pack(f'!BB{len(encoded_name)}s', SlotNumber.to_fw(slot), sense_type, encoded_name)
        return self.device.send_cmd_sync(Command.SET_SLOT_TAG_NICK, data)

    @expect_response(Status.SUCCESS)
    def get_slot_tag_nick(self, slot: SlotNumber, sense_type: TagSenseType):
        """
        Get the nick name of the slot.

        :param slot:  Card slot number
        :param sense_type:  field type
        :return:
        """
        # SlotNumber() will raise error for us if slot not in slot range
        data = struct.pack('!BB', SlotNumber.to_fw(slot), sense_type)
        resp = self.device.send_cmd_sync(Command.GET_SLOT_TAG_NICK, data)
        resp.parsed = resp.data.decode(encoding="utf8")
        return resp

    @expect_response(Status.SUCCESS)
    def get_all_slot_nicks(self):
        resp = self.device.send_cmd_sync(Command.GET_ALL_SLOT_NICKS, b'')

        slots = []
        i = 0
        slot_index = 0

        while i < len(resp.data) and slot_index < 8:
            slot_names = {'hf': '', 'lf': ''}

            if i < len(resp.data):
                hf_len = resp.data[i]
                i += 1
                if hf_len > 0 and i + hf_len <= len(resp.data):
                    slot_names['hf'] = resp.data[i:i + hf_len].decode(encoding="utf8", errors="ignore")
                    i += hf_len
                else:
                    i += hf_len

            if i < len(resp.data):
                lf_len = resp.data[i]
                i += 1
                if lf_len > 0 and i + lf_len <= len(resp.data):
                    slot_names['lf'] = resp.data[i:i + lf_len].decode(encoding="utf8", errors="ignore")
                    i += lf_len
                else:
                    i += lf_len

            slots.append(slot_names)
            slot_index += 1

        resp.parsed = slots
        return resp

    @expect_response(Status.SUCCESS)
    def delete_slot_tag_nick(self, slot: SlotNumber, sense_type: TagSenseType):
        """
        Delete the nick name of the slot.

        :param slot:  Card slot number
        :param sense_type:  field type
        :return:
        """
        # SlotNumber() will raise error for us if slot not in slot range
        data = struct.pack('!BB', SlotNumber.to_fw(slot), sense_type)
        return self.device.send_cmd_sync(Command.DELETE_SLOT_TAG_NICK, data)

    @expect_response(Status.SUCCESS)
    def mf1_get_emulator_config(self):
        """
            Get array of Mifare Classic emulators settings:
            [0] - mf1_is_detection_enable (mfkey32)
            [1] - mf1_is_gen1a_magic_mode
            [2] - mf1_is_gen2_magic_mode
            [3] - mf1_is_use_mf1_coll_res (use UID/BCC/SAK/ATQA from 0 block)
            [4] - mf1_get_write_mode

        :return:
        """
        resp = self.device.send_cmd_sync(Command.MF1_GET_EMULATOR_CONFIG)
        if resp.status == Status.SUCCESS:
            b1, b2, b3, b4, b5 = struct.unpack('!????B', resp.data)
            resp.parsed = {'detection': b1,
                           'gen1a_mode': b2,
                           'gen2_mode': b3,
                           'block_anti_coll_mode': b4,
                           'write_mode': b5}
        return resp

    @expect_response(Status.SUCCESS)
    def mf1_set_gen1a_mode(self, enabled: bool):
        """
        Set gen1a magic mode
        """
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF1_SET_GEN1A_MODE, data)

    @expect_response(Status.SUCCESS)
    def mf1_set_gen2_mode(self, enabled: bool):
        """
        Set gen2 magic mode
        """
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF1_SET_GEN2_MODE, data)

    @expect_response(Status.SUCCESS)
    def mf1_set_block_anti_coll_mode(self, enabled: bool):
        """
        Set 0 block anti-collision data
        """
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF1_SET_BLOCK_ANTI_COLL_MODE, data)

    @expect_response(Status.SUCCESS)
    def mf1_set_write_mode(self, mode: int):
        """
        Set write mode
        """
        data = struct.pack('!B', mode)
        return self.device.send_cmd_sync(Command.MF1_SET_WRITE_MODE, data)

    def mf1_get_prng_type(self):
        """
        Get PRNG type used for MF1 auth nonce:
          0 = Static  (fixed nonce)
          1 = Weak    (LFSR-based, predictable)
          2 = Hard    (unpredictable)
        """
        resp = self.device.send_cmd_sync(Command.MF1_GET_PRNG_TYPE)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def mf1_set_prng_type(self, prng_type: int):
        """
        Set PRNG type (0=Static, 1=Weak, 2=Hard)
        """
        data = struct.pack('!B', prng_type)
        return self.device.send_cmd_sync(Command.MF1_SET_PRNG_TYPE, data)

    @expect_response(Status.SUCCESS)
    def slot_data_config_save(self):
        """
        Update the configuration and data of the card slot to flash.
        :return:
        """
        return self.device.send_cmd_sync(Command.SLOT_DATA_CONFIG_SAVE)

    def enter_bootloader(self):
        """
        Reboot into DFU mode (bootloader)
        :return:
        """
        self.device.send_cmd_auto(Command.ENTER_BOOTLOADER, close=True)

    @expect_response(Status.SUCCESS)
    def get_animation_mode(self):
        """
        Get animation mode value
        """
        resp = self.device.send_cmd_sync(Command.GET_ANIMATION_MODE)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def get_enabled_slots(self):
        """
        Get enabled slots
        """
        resp = self.device.send_cmd_sync(Command.GET_ENABLED_SLOTS)
        if resp.status == Status.SUCCESS:
            resp.parsed = [{'hf': hf, 'lf': lf} for hf, lf in struct.iter_unpack('!BB', resp.data)]
        return resp

    @expect_response(Status.SUCCESS)
    def set_animation_mode(self, value: int):
        """
        Set animation mode value
        """
        data = struct.pack('!B', value)
        return self.device.send_cmd_sync(Command.SET_ANIMATION_MODE, data)

    @expect_response(Status.SUCCESS)
    def get_sleep_timeout(self):
        """
        Get the wake timeout (in seconds) after a button wakeup
        """
        resp = self.device.send_cmd_sync(Command.GET_SLEEP_TIMEOUT)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('!B', resp.data)[0]
        return resp

    @expect_response(Status.SUCCESS)
    def set_sleep_timeout(self, seconds: int):
        """
        Set the wake timeout (in seconds) after a button wakeup
        """
        data = struct.pack('!B', seconds)
        return self.device.send_cmd_sync(Command.SET_SLEEP_TIMEOUT, data)

    @expect_response(Status.SUCCESS)
    def reset_settings(self):
        """
        Reset settings stored in flash memory
        """
        resp = self.device.send_cmd_sync(Command.RESET_SETTINGS)
        resp.parsed = resp.status == Status.SUCCESS
        return resp

    @expect_response(Status.SUCCESS)
    def save_settings(self):
        """
        Store settings to flash memory
        """
        resp = self.device.send_cmd_sync(Command.SAVE_SETTINGS)
        resp.parsed = resp.status == Status.SUCCESS
        return resp

    @expect_response(Status.SUCCESS)
    def wipe_fds(self):
        """
        Reset to factory settings
        """
        resp = self.device.send_cmd_sync(Command.WIPE_FDS)
        resp.parsed = resp.status == Status.SUCCESS
        self.device.close()
        return resp

    @expect_response(Status.SUCCESS)
    def get_battery_info(self):
        """
        Get battery info
        """
        resp = self.device.send_cmd_sync(Command.GET_BATTERY_INFO)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('!HB', resp.data)
        return resp

    @expect_response(Status.SUCCESS)
    def get_button_press_config(self, button: ButtonType):
        """
        Get config of button press function
        """
        data = struct.pack('!B', button)
        resp = self.device.send_cmd_sync(Command.GET_BUTTON_PRESS_CONFIG, data)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def set_button_press_config(self, button: ButtonType, function: ButtonPressFunction):
        """
        Set config of button press function
        """
        data = struct.pack('!BB', button, function)
        return self.device.send_cmd_sync(Command.SET_BUTTON_PRESS_CONFIG, data)

    @expect_response(Status.SUCCESS)
    def get_long_button_press_config(self, button: ButtonType):
        """
        Get config of long button press function
        """
        data = struct.pack('!B', button)
        resp = self.device.send_cmd_sync(Command.GET_LONG_BUTTON_PRESS_CONFIG, data)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def set_long_button_press_config(self, button: ButtonType, function: ButtonPressFunction):
        """
        Set config of long button press function
        """
        data = struct.pack('!BB', button, function)
        return self.device.send_cmd_sync(Command.SET_LONG_BUTTON_PRESS_CONFIG, data)

    @expect_response(Status.SUCCESS)
    def set_ble_connect_key(self, key: str):
        """
        Set config of ble connect key
        """
        data_bytes = key.encode(encoding='ascii')

        # check key length
        if len(data_bytes) != 6:
            raise ValueError("The ble connect key length must be 6")

        data = struct.pack('6s', data_bytes)
        return self.device.send_cmd_sync(Command.SET_BLE_PAIRING_KEY, data)

    @expect_response(Status.SUCCESS)
    def get_ble_pairing_key(self):
        """
        Get config of ble connect key
        """
        resp = self.device.send_cmd_sync(Command.GET_BLE_PAIRING_KEY)
        resp.parsed = resp.data.decode(encoding='ascii')
        return resp

    @expect_response(Status.SUCCESS)
    def delete_all_ble_bonds(self):
        """
        From peer manager delete all bonds.
        """
        return self.device.send_cmd_sync(Command.DELETE_ALL_BLE_BONDS)

    @expect_response(Status.SUCCESS)
    def get_device_capabilities(self):
        """
        Get list of commands that client understands
        """
        try:
            resp = self.device.send_cmd_sync(Command.GET_DEVICE_CAPABILITIES)
        except chameleon_com.CMDInvalidException:
            print("Chameleon does not understand get_device_capabilities command. Please update firmware")
            return chameleon_com.Response(cmd=Command.GET_DEVICE_CAPABILITIES,
                                          status=Status.NOT_IMPLEMENTED)
        else:
            if resp.status == Status.SUCCESS:
                resp.parsed = [x[0] for x in struct.iter_unpack('!H', resp.data)]
            return resp

    @expect_response(Status.SUCCESS)
    def get_device_model(self):
        """
        Get device model
        0 - Chameleon Ultra
        1 - Chameleon Lite
        """

        resp = self.device.send_cmd_sync(Command.GET_DEVICE_MODEL)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def get_device_settings(self):
        """
        Get all possible settings
        For version 6:
        settings[0] = SETTINGS_CURRENT_VERSION; // current version
        settings[1] = settings_get_animation_config(); // animation mode
        settings[2] = settings_get_button_press_config('A'); // short A button press mode
        settings[3] = settings_get_button_press_config('B'); // short B button press mode
        settings[4] = settings_get_long_button_press_config('A'); // long A button press mode
        settings[5] = settings_get_long_button_press_config('B'); // long B button press mode
        settings[6] = settings_get_ble_pairing_enable(); // does device require pairing
        settings[7:13] = settings_get_ble_pairing_key(); // BLE pairing key
        settings[13] = sleep_timeout in seconds; // wake timeout after button wakeup
        """
        resp = self.device.send_cmd_sync(Command.GET_DEVICE_SETTINGS)
        if resp.status == Status.SUCCESS:
            if resp.data[0] > CURRENT_VERSION_SETTINGS:
                raise ValueError("Settings version in app older than Chameleon. "
                                 "Please upgrade client")
            if resp.data[0] < CURRENT_VERSION_SETTINGS:
                raise ValueError("Settings version in app newer than Chameleon. "
                                 "Please upgrade Chameleon firmware")
            settings_version, animation_mode, btn_press_A, btn_press_B, btn_long_press_A, \
                btn_long_press_B, ble_pairing_enable, ble_pairing_key, sleep_timeout = \
                struct.unpack('!BBBBBBB6sB', resp.data)
            resp.parsed = {'settings_version': settings_version,
                           'animation_mode': animation_mode,
                           'btn_press_A': btn_press_A,
                           'btn_press_B': btn_press_B,
                           'btn_long_press_A': btn_long_press_A,
                           'btn_long_press_B': btn_long_press_B,
                           'ble_pairing_enable': ble_pairing_enable,
                           'ble_pairing_key': ble_pairing_key,
                           'sleep_timeout': sleep_timeout}
        return resp

    @expect_response(Status.SUCCESS)
    def hf14a_get_anti_coll_data(self):
        """
        Get anti-collision data from current HF slot (UID/SAK/ATQA/ATS)

        :return:
        """
        resp = self.device.send_cmd_sync(Command.HF14A_GET_ANTI_COLL_DATA)
        if resp.status == Status.SUCCESS and len(resp.data) > 0:
            # uidlen[1]|uid[uidlen]|atqa[2]|sak[1]|atslen[1]|ats[atslen]
            offset = 0
            uidlen, = struct.unpack_from('!B', resp.data, offset)
            offset += struct.calcsize('!B')
            uid, atqa, sak, atslen = struct.unpack_from(f'!{uidlen}s2s1sB', resp.data, offset)
            offset += struct.calcsize(f'!{uidlen}s2s1sB')
            ats, = struct.unpack_from(f'!{atslen}s', resp.data, offset)
            offset += struct.calcsize(f'!{atslen}s')
            resp.parsed = {'uid': uid, 'atqa': atqa, 'sak': sak, 'ats': ats}
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_uid_magic_mode(self):
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_UID_MAGIC_MODE)
        if resp.status == Status.SUCCESS:
            resp.parsed, = struct.unpack('!?', resp.data)
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_set_uid_magic_mode(self, enabled: bool):
        return self.device.send_cmd_sync(Command.MF0_NTAG_SET_UID_MAGIC_MODE, struct.pack('?', enabled))

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_version_data(self):
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_VERSION_DATA)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[:8]
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_set_version_data(self, data: bytes):
        assert len(data) == 8
        return self.device.send_cmd_sync(Command.MF0_NTAG_SET_VERSION_DATA, data)

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_signature_data(self):
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_SIGNATURE_DATA)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[:32]
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_set_signature_data(self, data: bytes):
        assert len(data) == 32
        return self.device.send_cmd_sync(Command.MF0_NTAG_SET_SIGNATURE_DATA, data)

    @expect_response(Status.SUCCESS)
    def mf0_ntag_get_write_mode(self):
        """
        Get write mode for MF0/NTAG
        """
        resp = self.device.send_cmd_sync(Command.MF0_NTAG_GET_WRITE_MODE)
        if resp.status == Status.SUCCESS:
            resp.parsed = resp.data[0]
        return resp

    @expect_response(Status.SUCCESS)
    def mf0_ntag_set_write_mode(self, mode: int):
        """
        Set write mode for MF0/NTAG
        """
        data = struct.pack('!B', mode)
        return self.device.send_cmd_sync(Command.MF0_NTAG_SET_WRITE_MODE, data)

    @expect_response(Status.SUCCESS)
    def get_ble_pairing_enable(self):
        """
        Is ble pairing enable?

        :return: True if pairing is enable, False if pairing disabled
        """
        resp = self.device.send_cmd_sync(Command.GET_BLE_PAIRING_ENABLE)
        if resp.status == Status.SUCCESS:
            resp.parsed, = struct.unpack('!?', resp.data)
        return resp

    @expect_response(Status.SUCCESS)
    def set_ble_pairing_enable(self, enabled: bool):
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.SET_BLE_PAIRING_ENABLE, data)

    @expect_response(Status.SUCCESS)
    def mf1_get_field_off_do_reset(self):
        resp = self.device.send_cmd_sync(Command.MF1_GET_FIELD_OFF_DO_RESET)
        if resp.status == Status.SUCCESS:
            resp.parsed = struct.unpack('!B', resp.data)[0] == 1
        return resp

    @expect_response(Status.SUCCESS)
    def mf1_set_field_off_do_reset(self, enabled: bool):
        data = struct.pack('!B', enabled)
        return self.device.send_cmd_sync(Command.MF1_SET_FIELD_OFF_DO_RESET, data)

    @expect_response(Status.SUCCESS)
    def seos_read_emu_data(self):
        resp = self.device.send_cmd_sync(Command.SEOS_READ_EMU_DATA, None)
        resp.parsed = {}

        def extract_next():
            length = resp.data[0]
            value = resp.data[1:length+1]
            resp.data = resp.data[length+1:]
            return value

        data, oid, tag, diversifier = extract_next(), extract_next(), extract_next(), extract_next()
        hash_alg, encr_alg = struct.unpack('!BB', resp.data)

        resp.parsed = {
            "data": data, "oid": oid, "tag": tag, "diversifier": diversifier,
            "hash_alg": hash_alg, "encr_alg": encr_alg
        }

        return resp

    @expect_response(Status.SUCCESS)
    def seos_write_emu_data(self, data: bytes, oid: bytes, tag: bytes, diversifier: bytes, hash_alg: int, encr_alg: int):
        data = bytes([len(data)]) + data
        oid = bytes([len(oid)]) + oid
        tag = bytes([len(tag)]) + tag
        diversifier = bytes([len(diversifier)]) + diversifier
        
        payload = (
            data + oid + tag + diversifier +
            struct.pack('!BB', hash_alg, encr_alg)
        )
        
        if len(payload) > 4096:
            raise ValueError("Too much provided data")

        return self.device.send_cmd_sync(Command.SEOS_WRITE_EMU_DATA, payload)
    
    @expect_response(Status.SUCCESS)
    def seos_write_emu_keys(self, auth: bytes, privenc: bytes, privmac: bytes):
        payload = auth + privenc + privmac
        return self.device.send_cmd_sync(Command.SEOS_WRITE_EMU_KEYS, payload)


def test_fn():
    # connect to chameleon
    dev = chameleon_com.ChameleonCom()
    try:
        dev.open('com19')
    except chameleon_com.OpenFailException:
        dev.open('/dev/ttyACM0')

    cml = ChameleonCMD(dev)
    ver = cml.get_app_version()
    print(f"Firmware number of application: {ver[0]}.{ver[1]}")
    chip = cml.get_device_chip_id()
    print(f"Device chip id: {chip}")

    # change to reader mode
    cml.set_device_reader_mode()

    options = {
        'activate_rf_field': 1,
        'wait_response': 1,
        'append_crc': 0,
        'auto_select': 0,
        'keep_rf_field': 1,
        'check_response_crc': 0,
    }

    try:
        # unlock 1
        resp = cml.hf14a_raw(options=options, resp_timeout_ms=1000, data=[0x40], bitlen=7)

        if resp[0] == 0x0a:
            print("Gen1A unlock 1 success")
            # unlock 2
            resp = cml.hf14a_raw(options=options, resp_timeout_ms=1000, data=[0x43])
            if resp[0] == 0x0a:
                print("Gen1A unlock 2 success")
                print("Start dump gen1a memory...")
                # Transfer with crc
                options['append_crc'] = 1
                options['check_response_crc'] = 1
                block = 0
                while block < 64:
                    # Tag read block cmd
                    cmd_read_gen1a_block = [0x30, block]
                    if block == 63:
                        options['keep_rf_field'] = 0
                    resp = cml.hf14a_raw(options=options, resp_timeout_ms=100, data=cmd_read_gen1a_block)

                    print(f"Block {block} : {resp.hex()}")
                    block += 1

            else:
                print("Gen1A unlock 2 fail")
                raise
        else:
            print("Gen1A unlock 1 fail")
            raise
    except Exception:
        options['keep_rf_field'] = 0
        options['wait_response'] = 0
        cml.hf14a_raw(options=options)

    # disconnect
    dev.close()


if __name__ == '__main__':
    test_fn()
