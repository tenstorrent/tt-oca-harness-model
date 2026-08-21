# Key Manager (KM) Firmware

This section describes the current ROM firmware revision for the KM subsystem. The KM
firmware runs on the PicoRV32 and is responsible for bringing the subsystem to a secure
operational state, servicing SEP mailbox commands, managing key material in the KPV,
sideloading keys into approved crypto engines, and handling recoverable and unrecoverable
fault conditions. In this revision, the ROM firmware includes the following major functional areas:
● Boot and secure initialization
● Mailbox command/response protocol
● Message receive/transmit buffering
● Command parsing and validation
● Interrupt and fault handling
● DRBG and firmware PRNG support
● KPV and KPVLP management
● Crypto-engine sideload drivers for HMAC, KMAC, AES, and OTBN
● CRC support functions
● Key registry and key lifecycle management
● Main event loop and runtime state management
The firmware supports the SEP-facing command set for version and status queries, KPVLP slot
allocation and key registration, random key generation, key transfer, key revocation, engine
shredding, and recoverable-fault acknowledgement. In this ROM revision, CMD_SRAM_VER is
reserved for a future SRAM firmware flow and must always return failure.

## Firmware Global Parameters

SHRED_ITER indicates the number of extra overwrite passes used during shred operations.
Any shred sequence therefore performs SHRED_ITER + 1 total passes.

## Command Message Reference

The SEP communicates with the KM through a framed command/response protocol carried over
the mailbox FIFOs. Every direct SEP command elicits a RESP_CMD response from the KM.
The KM may also emit unsolicited responses, including RESP_KM_READY,
RESP_RECOVERABLE_FAULT, and RESP_UNRECOVERABLE_FAULT.
All reserved fields read as 0 and must be written as 0. The maximum payload length for a single
message is 255 32-bit words (1020 bytes). The message header CRC and payload CRC are
validated independently before command execution. Header validation, sequence checking,


command decoding, payload-length checking, and payload CRC checking all occur before a
command handler is allowed to act on the payload.
All command messages use the following container format.
Command Message
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
HEADER_CRC8[31:24] PAYLOAD_LEN[23:16] COMMAND_ID[15:8] CMD_SEQ_NUM[7:0]
PAYLOAD_WORD[0][31:0]
...
PAYLOAD_WORD[n-1][31:0]
PAYLOAD_CRC32[31:0]
Message Field Description
**Field Description**
CMD_SEQ_NUM The command message sequence number. It is reset to zero during initialization or
during any message buffer flush. This value must increment with each message. It must
roll over to 0 after reaching the maximum value.
COMMAND_ID The ID of the command for the KM to execute. This determines how the
PAYLOAD_WORDs are interpreted.
PAYLOAD_LEN The length of the message payload in number of 32-bit words, _not_ including the
PAYLOAD_CRC32. A payload length of 0 indicates the message has no payload.
HEADER_CRC8 A CRC-8/ROHC computed over the 24-bit message header {PAYLOAD_LEN,
COMMAND_ID, CMD_SEQ_NUM} in little endian order, starting with CMD_SEQ_NUM.
PAYLOAD_WORD[n] A 32-bit data word for the command message payload, where _n_ is always less than
PAYLOAD_LEN. PAYLOAD_LEN equal to 0 implies no PAYLOAD_WORDs are
present. The contents of the PAYLOAD_WORDs are command-specific.
PAYLOAD_CRC32 A CRC-32C (Castagnoli) computed from the command payload in little endian order
from PAYLOAD_WORD[0] to PAYLOAD_WORD[n-1]. PAYLOAD_LEN equal to 0
implies no PAYLOAD_CRC32 is present.
The rest of this section describes all valid commands and their payloads.
0x00 - CMD_HW_VER
Description
Requests KM hardware version information.


Payload
This command has no payload.
Response
This command has a RESP_CMD response with a return code of _success_ , and a return
argument containing the version of the hardware.
RESP_CMD _Success_ Return Argument
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED HW_MAJOR_REV[23:16] HW_MINOR_REV[15:8] HW_PATCH_REV[7:0]
0x01 - CMD_ROM_VER
Description
Requests KM ROM firmware version information.
Payload
This command has no payload.
Response
This command has a RESP_CMD response with a return code of _success_ , and a return
argument containing the version of the ROM firmware.
RESP_CMD _Success_ Return Argument
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED ROM_MAJOR_REV[23:16] ROM_MINOR_REV[15:8] ROM_PATCH_REV[7:0]
0x02 - CMD_SRAM_VER
Description
Requests KM SRAM firmware version information (if any).
Payload
This command has no payload.


Response
If no SRAM firmware is loaded, this command has a RESP_CMD response with a return code of
_failure_ , and no return argument.
If SRAM firmware is loaded and executing, this command has a RESP_CMD response with a
return code of _success_ , and a return argument containing the version of the SRAM firmware.
RESP_CMD _Success_ Return Argument
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED SRAM_MAJOR_REV[23:16] SRAM_MINOR_REV[15:8] SRAM_PATCH_REV[7:0]
0x03 - CMD_STAT
Description
Requests KM status information.
Payload
This command has no payload.
Response
This command has a RESP_CMD response with a return code of _success_ , and a return
argument containing the KM status information.
RESP_CMD _Success_ Return Argument
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
KM_STATUS[31:0]
KM_STATUS Payload
**Bit Name Description**
0 recov_fault The value of the RECOVERABLE_ERR register bit. Set to 1 if the KM is waiting for the
SEP to acknowledge a recoverable fault.
[31:1] reserved All other fields reserved.


0x04 - CMD_RECOV_ACK
Description
Acknowledges a recoverable error event in the KM. This command clears the
RECOVERABLE_ERR bit in the KMCSR.
Payload
This command has no payload.
Response
This command has a RESP_CMD response with a return code of _success_ , and no return
argument.
0x05 - CMD_EXEC_ROM
**_THIS SECTION IS WIP_**
Description
Sets the KM into ROM execution mode. Once set, the KM will not accept SRAM firmware load
or execution until the next cold or warm reset.
Payload
The KM will only
Command Payload
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED MODE[1:0]^
0x06 - CMD_FIRM
**_THIS SECTION IS WIP_**
Description
Initiates firmware load for
Payload
The execution mode of the KM and the base address to begin writing the firmware image
relative to the SRAM base address, as well as the system-relative starting address for the
loaded firmware. The base image address and jump address must be 32-bit word aligned.


```
Immediately after receiving this command, the KM will begin transferring the raw words of the
next mailbox frame into the SRAM, starting at the provided base address. Since writing the
image to SRAM is a destructive operation, this code must be written in assembly and must
restrict its working state to local CPU registers. It will follow the following sequence:
```
1. Do the following in a loop:
    a. Read a word from the mailbox.
    b. Check the separator status bit.
    c. If the separator bit is set, the word that was just read is the CRC for the image.
       Break from the loop.
    d. Write the word into the next SRAM location, where the first location starts at the
       provided IMAGE_BASE_ADDRESS.
    e. Read the word back from SRAM and compute the next CRC state from this
       value.
2. Compare the word that was just read from the mailbox with the computed CRC state. If
    they don’t match, trigger an unrecoverable fault.
3. Jump to the provided JUMP_ADDRESS.
    Command Payload
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED MODE[1:0]^
    RESERVED IMAGE_BASE_ADDRESS[13:0]
       JUMP_ADDRESS[31:0]
Response
This command has no response.
Just prior to jumping into the SRAM firmware at the provided address, the computed CRC is
compared to the provided CRC word at the end of the firmware payload. If the check fails, the
KM will trigger an unrecoverable error.
0x20 - CMD_KPVLP_SLOT_REQ
Description
Requests unused slots from the KPV for key loading via the KPVLP.
Payload
The number of consecutive slots requested (equal to SLOT_REQ+1).


Command Payload
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED SLOT_REQ[2:0]
Response
If any arguments are invalid, this command has a RESP_CMD response with a return code of
_invalid_arg_ , and a return argument pointing to the index of the invalid payload word.
If the request cannot be satisfied for any other reason, this command has a RESP_CMD
response with a return code of _failure_ , and no return argument. This command will respond with
_failure_ if the KPV does not have SLOT_REQ+1 number of consecutive free slots available to
grant.
If the request is granted, this command has a RESP_CMD response with a return code of
_success_ , and a return argument containing the base index of the granted slot(s) and the granted
number of consecutive slots (equal to SLOT_GRANT+1). SLOT_GRANT must always be
equivalent to SLOT_REQ. Once granted, the KM will not use or register the SEP-allocated key
slot for any other purpose except CMD_KPVLP_KEY_REGISTER key registration.
RESP_CMD _Success_ Return Argument
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED SLOT_GRANT[10:8]^ RESERVED BASE_SLOT_INDEX[4:0]
0x21 - CMD_KPVLP_KEY_REGISTER
Description
Following a write into the KPVLP, this command registers and write-locks the associated slot(s)
in the KPV. Since the SEP cannot read back written key data (the KPVLP key registers are
read-only), for validation the SEP must provide the key parameters and a CRC-32C of the key
data. The KM will confirm or deny the request when it’s complete and return a key handle.
Once this command is called, the SEP must not write to any key slot. The KM will validate that
the KPV requested key slot was not changed during this key registration process, and will return
an error if it detects tampering.
Payload
The SEP must provide the base slot index of the key to register, its size in multiples of 32-bit
words (where the key size is KEY_SIZE+1), and a CRC-32C of the key data, as well as the valid


destinations for the key. The KM will check the slot control fields and validate the key data
CRC-32C with the contents of the KPV slot(s).
Command Payload
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED BASE_SLOT_INDEX[4:0]
RESERVED KEY_SIZE[6:0]
RESERVED DEST_VALID[7:0]
KEY_CRC32[31:0]
DEST_VALID Payload
**Bit Name Description**
0 hmac_sha2 Key transfer to the HMAC engine is permitted when set to 1.
1 kmac_sha3 Key transfer to the KMAC engine is permitted when set to 1.
2 aes Key transfer to the AES engine is permitted when set to 1.
3 otbn Key transfer to the OTBN engine is permitted when set to 1.
4 abr_mldsa_seed Key transfer to Adams Bridge ML-DSA seed is permitted when set to 1.
5 abr_mlkem_d Key transfer to Adams Bridge ML-KEM D is permitted when set to 1.
6 abr_mlkem_z Key transfer to Adams Bridge ML-KEM Z is permitted when set to 1.
7 abr_mlkem_msg Key transfer to Adams Bridge ML-KEM MSG is permitted when set to 1.
Response
If any arguments are invalid, this command has a RESP_CMD response with a return code of
_invalid_arg_ , and a return argument pointing to the index of the invalid payload word.
If the provided key control data does not correspond with the KPV control data or the CRC-32C
does not check out, this command has a RESP_CMD response with a return code of _failure_ ,
and no return argument. The SEP must not attempt to write key data to a write-locked or
non-SEP allocated key slot. The SEP must also not attempt any writes to the KPVLP key slots
targeted by this command while this command is pending. Any such attempts will result in a
command response of _failure_.
If the request is granted, this command has a RESP_CMD response with a return code of
_success_ , and a return argument containing the handle for the registered key.


RESP_CMD _Success_ Return Argument
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED KEY_HANDLE[7:0]
0x22 - CMD_KEY_GENERATE
Description
Requests a freshly generated random key.
Payload
The size of the requested key in multiples of 32-bit words (equal to REQ_SIZE+1), as well as
the valid destinations for the key.
Command Payload
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED REQ_SIZE[6:0]
RESERVED DEST_VALID[7:0]
DEST_VALID Payload
**Bit Name Description**
0 hmac_sha2 Key transfer to the HMAC engine is permitted when set to 1.
1 kmac_sha3 Key transfer to the KMAC engine is permitted when set to 1.
2 aes Key transfer to the AES engine is permitted when set to 1.
3 otbn Key transfer to the OTBN engine is permitted when set to 1.
4 abr_mldsa_seed Key transfer to Adams Bridge ML-DSA seed is permitted when set to 1.
5 abr_mlkem_d Key transfer to Adams Bridge ML-KEM D is permitted when set to 1.
6 abr_mlkem_z Key transfer to Adams Bridge ML-KEM Z is permitted when set to 1.
7 abr_mlkem_msg Key transfer to Adams Bridge ML-KEM MSG is permitted when set to 1.
Response
If any arguments are invalid, this command has a RESP_CMD response with a return code of
_invalid_arg_ , and a return argument pointing to the index of the invalid payload word.


The KM will respond with _failure_ if the KPV does not have REQ_SIZE+1 number of consecutive
free slots available to grant.
If the request is granted, this command has a RESP_CMD response with a return code of
_success_ , and a return argument containing the parameters of the generated key, including its
handle, its size, and its valid destinations. The granted size must always be equivalent to the
requested size.
RESP_CMD _Success_ Return Argument
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED DEST_VALID[23:16] REQ_SIZE[14:8] KEY_HANDLE[7:0]
0x23 - CMD_KEY_REVOKE
Description
This command will revoke an existing key from the KPV, given a valid key handle.
Payload
The handle for the key to invalidate.
Command Payload
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED KEY_HANDLE[7:0]
Response
If any arguments are invalid, this command has a RESP_CMD response with a return code of
_invalid_arg_ , and a return argument pointing to the index of the invalid payload word.
If the request cannot be satisfied for any other reason, this command has a RESP_CMD
response with a return code of _failure_ , and no return argument.
If the request is granted, this command has a RESP_CMD response with a return code of
_success_ , and a return argument containing the handle of the revoked key.
RESP_CMD _Success_ Return Argument


[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED KEY_HANDLE[7:0]
0x24 - CMD_KEY_TRANSFER
Description
This command transfers a key from the KPV to one or more crypto engines, given a valid key
handle and a bitfield of requested destinations. Requested destination crypto engines must be
permitted by the corresponding bit in the DEST_VALID field of the KPV slot.
Payload
The handle of the key to transfer in the KPV and the bitmask of the destination crypto engines.
Command Payload
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED KEY_HANDLE[7:0]
RESERVED DEST_ENGINE[7:0]
DEST_ENGINE Payload
**Bit Name Description**
0 hmac_sha2 Key transfer to the HMAC engine is requested when set to 1.
1 kmac_sha3 Key transfer to the KMAC engine is requested when set to 1.
2 aes Key transfer to the AES engine is requested when set to 1.
3 otbn Key transfer to the OTBN engine is requested when set to 1.
4 abr_mldsa_seed Key transfer to Adams Bridge ML-DSA seed is permitted when set to 1.
5 abr_mlkem_d Key transfer to Adams Bridge ML-KEM D is permitted when set to 1.
6 abr_mlkem_z Key transfer to Adams Bridge ML-KEM Z is permitted when set to 1.
7 abr_mlkem_msg Key transfer to Adams Bridge ML-KEM MSG is permitted when set to 1.


Response
If any arguments are invalid, this command has a RESP_CMD response with a return code of
_invalid_arg_ , and a return argument pointing to the index of the invalid payload word.
If the request cannot be satisfied for any other reason, this command has a RESP_CMD
response with a return code of _failure_ , and no return argument. If _any_ requested crypto engines
are disallowed, the request will result in _failure_ and no requested key transfers will take place.
If the request is granted, this command has a RESP_CMD response with a return code of
_success_ , and a return argument containing the key handle and a bitfield of the crypto engines to
which the key was transferred.
RESP_CMD _Success_ Return Argument
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED DEST_ENGINE[15:8] KEY_HANDLE[7:0]
DEST_ENGINE Payload
**Bit Name Description**
0 hmac_sha2 Key transfer to the HMAC engine was completed when set to 1.
1 kmac_sha3 Key transfer to the KMAC engine was completed when set to 1.
2 aes Key transfer to the AES engine was completed when set to 1.
3 otbn Key transfer to the OTBN engine was completed when set to 1.
4 abr_mldsa_seed Key transfer to Adams Bridge ML-DSA seed is permitted when set to 1.
5 abr_mlkem_d Key transfer to Adams Bridge ML-KEM D is permitted when set to 1.
6 abr_mlkem_z Key transfer to Adams Bridge ML-KEM Z is permitted when set to 1.
7 abr_mlkem_msg Key transfer to Adams Bridge ML-KEM MSG is permitted when set to 1.
0x25 - CMD_ENGINE_SHRED
Description
This command will purge the key data registers from one or multiple crypto engines. These
registers will be scrambled with random data. Any number of engines may be selected.


Payload
A bitfield indicating the crypto engines to purge.
Command Payload
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED DEST_ENGINE[7:0]
DEST_ENGINE Payload
**Bit Name Description**
0 hmac_sha2 Key purging of the HMAC engine is requested when set to 1.

1 kmac_sha3 (^) Key purging of the KMAC engine is requested when set to 1.
2 aes Key purging of the AES engine is requested when set to 1.
3 otbn Key purging of the OTBN engine is requested when set to 1.
4 abr_mldsa_seed Key transfer to Adams Bridge ML-DSA seed is permitted when set to 1.
5 abr_mlkem_d Key transfer to Adams Bridge ML-KEM D is permitted when set to 1.
6 abr_mlkem_z Key transfer to Adams Bridge ML-KEM Z is permitted when set to 1.
7 abr_mlkem_msg Key transfer to Adams Bridge ML-KEM MSG is permitted when set to 1.
Response
If any arguments are invalid, this command has a RESP_CMD response with a return code of
_invalid_arg_ , and a return argument pointing to the index of the invalid payload word.
If the request cannot be satisfied for any other reason, this command has a RESP_CMD
response with a return code of _failure_ , and no return argument.
If the request is granted, this command has a RESP_CMD response with a return code of
_success_ , and a return argument indicating the crypto engines that were purged.
RESP_CMD _Success_ Return Argument
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED DEST_ENGINE[7:0]


DEST_ENGINE Payload
**Bit Name Description**
0 hmac_sha2 Key material of the HMAC engine was purged when set to 1.
1 kmac_sha3 Key material of the KMAC engine was purged when set to 1.
2 aes Key material of the AES engine was purged when set to 1.
3 otbn Key material of the OTBN engine was purged when set to 1.
4 abr_mldsa_seed Key transfer to Adams Bridge ML-DSA seed is permitted when set to 1.
5 abr_mlkem_d Key transfer to Adams Bridge ML-KEM D is permitted when set to 1.
6 abr_mlkem_z Key transfer to Adams Bridge ML-KEM Z is permitted when set to 1.
7 abr_mlkem_msg Key transfer to Adams Bridge ML-KEM MSG is permitted when set to 1.
Response Message Reference
Response Message
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
HEADER_CRC8[31:24] PAYLOAD_LEN[23:16] RESPONSE_ID[15:8] RESP_SEQ_NUM[7:0]
PAYLOAD_WORD[0][31:0]
...
PAYLOAD_WORD[n-1][31:0]
PAYLOAD_CRC32[31:0]
The KM sends response messages to the SEP using the exact same basic message container
format as command messages.
The sequence numbers of response messages do not correspond to the sequence numbers of
command messages. There is also no direct relationship between command IDs and response
IDs.
0x00 - RESP_CMD
Description
A general command response message. The key manager responds to all direct commands
from the SEP with this response.


Payload
The sequence number from the source command, the source command ID, a signed 8-bit return
code, and an optional argument.
Response Payload
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED CMD_SEQ_NUM[7:0]
RESERVED COMMAND_ID[7:0]
RESERVED RETURN_CODE[7:0]
RETURN_ARG[31:0] (Optional)
Return Codes
**Code Name Description Argument Value**
0 success General success code. Specific to the type of source COMMAND_ID.
-1 failure General failure code. Specific to the type of source COMMAND_ID.
-2 header_crc The header CRC-8/ROHC of the source command was invalid. No action was taken. The actual CRC-8/ROHC of the header that was received.
-3 cmd_noseq The CMD_SEQ_NUM of the source command was not the expected sequence number.
No action was taken.
The next expected command sequence number.
-4 invalid_cmd COMMAND_ID did not map to a valid command. No action was taken. None
-5 invalid_len The PAYLOAD_LEN of the source command did not correspond to the size of the
mailbox message frame. No action was taken.
The actual length of the message payload that was received.
-6 payload_crc The payload CRC-32C of the source command was invalid. No action was taken. The actual CRC-32C of the payload that was received.
-7 invalid_arg One or more payload arguments of the source command were invalid. No action was
taken.
The index for the payload word of the received command with the invalid
argument. If the index points to a location outside of the received payload
arguments, it indicates an argument was missing.
0x55 - RESP_KM_READY
Description
An unsolicited response message that indicates the KM has completed initialization and has
entered its main event loop. Serves as a proof of life indicator for the SEP.


Payload
This command has no payload.
0xFE - RESP_RECOVERABLE_FAULT
Description
An unsolicited response message that indicates the KM has encountered a recoverable fault.
Payload
A signed 8-bit code indicating the cause of the fault.
Response Payload
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED FAULT_CODE[7:0]
Fault Codes
**Code Name Description**
-1 key_slot_crc The CRC-32C check for a key slot has failed. Whatever operation triggered the fault
was aborted.
-2 rx_buff_oflow The message receive buffer has encountered a message overflow. This is caused by a
singular incomplete message filling up the message receive buffer. All message buffers
and mailbox FIFOs have been flushed by the KM.
-3 mbox_overflow The KM outgoing mailbox FIFO has encountered an overflow condition. All message
buffers and mailbox FIFOs have been flushed by the KM.
-4 mbox_underflow The KM incoming mailbox FIFO has encountered an underflow condition. All message
buffers and mailbox FIFOs have been flushed by the KM.
-5 flushed_by_sep The KM mailbox was flushed by the SEP. All message buffers have been flushed by the
KM. The mailbox FIFOs are not flushed.
0xFF - RESP_UNRECOVERABLE_FAULT
Description
An unsolicited response message that indicates the KM has encountered an unrecoverable
fault. Core execution has halted.


Payload
A signed 8-bit code indicating the cause of the fault.
Response Payload
[31] [30] [29] [28] [27] [26] [25] [24] [23] [22] [21] [20] [19] [18] [17] [16] [15] [14] [13] [12] [11] [10] [9] [8] [7] [6] [5] [4] [3] [2] [1] [0]
RESERVED FAULT_CODE[7:0]
Fault Codes
**Code Name Description**
-1 wipe_state The wipe state signal was asserted and the KM has purged all sensitive data

-2 rom_parity (^) The ROM encountered a parity error
-3 sram_parity The SRAM encountered a parity error
-4 rom_write The ROM encountered a write error
-5 sram_write_lock The SRAM encountered a write error to a write-locked region
-6 axi_decerr The KM system fabric encountered an AXI DECERR
-7 axi_slverr The KM system fabric encountered an unhandled AXI SLVERR
-8 drbg_err There was an error reading from the DRBG
-9 illegal_insn An illegal instruction was encountered
-10 bus_error The CPU core encountered an internal bus error or misaligned access
-11 ebreak An EBREAK instruction was executed
-12 spurious_irq An unrecognised IRQ source was asserted
Common Sequences
Initial Boot
As soon as the KM is taken out of reset, it begins executing out of the ROM. The ROM performs
basic setup routines such as initialization of peripherals and their drivers, as well as scrambling
sensitive memory locations like the KPV and crypto engine sideload key registers. Once the
boot process is complete, the KM sends a liveness message to the SEP via RESP_KM_READY
and waits for further instruction via mailbox commands.


At this point the SEP may want to query hardware and/or firmware version and status via the
CMD_HW_VER, CMD_ROM_VER, and CMD_STAT.

### SEP Key Loading

The KM supports loading keys directly from the SEP via the KPVLP. The typical flow is as
follows:

1. The SEP issues a request for a key slot in the KPV via the CMD_KPVLP_SLOT_REQ
    command, then waits for a response from the KM that includes the allocated KPV slot.
2. The SEP writes the key into the allocated KPV slot via the KPVLP interface. Writes to
    any unallocated slots are rejected with a SLVERR.
3. The SEP issues a request to register its key via the CMD_KPVLP_KEY_REGISTER
    command, then waits for the KM to validate key parameters and respond with a handle
    for the key.
4. The SEP may now use the provided key handle for any further key management
    operations.

### Key Transfer and Sideload Purge

The SEP requests transfer of a key from the KPV to any crypto engine sideload key register via
the CMD_KEY_TRANSFER command. The SEP provides a handle for the key and a bitmask
indicating the destination sideload key registers. The KM will respond with _success_ when the
operation is complete. If the key handle is invalid or any requested destination sideload key
registers are disallowed, the KM will respond with _failure_.
When the SEP is done with the crypto engine, it issues the CMD_ENGINE_SHRED command
to purge key material in the key sideload registers. The KM will respond with _success_ when the
operation is complete.

### Key Revocation

The SEP requests revocation of a key in the KPV via the CMD_KEY_REVOKE command. The
KM will read-lock the key and remove the key from the key registry. It is now no longer
accessible or transferable by either the SEP or KM.

### Error Recovery

The KM can encounter a variety of recoverable and unrecoverable errors.
A recoverable error signals a condition that puts the KM into a permanent halt state. It has the
following flow:

1. The KM asserts the recoverable error signal.
2. The KM flushes the mailbox _only_ for any recoverable error involving the mailbox or
    message buffers.


3. The KM sends the RESP_RECOVERABLE_FAULT response message to the SEP with
    a code indicating the cause of the fault.
4. The KM will _not_ process any command **except** :
    a. CMD_HW_VER
    b. CMD_ROM_VER
    c. CMD_SRAM_VER
    d. CMD_STAT
    e. CMD_RECOV_ACK
5. The SEP takes any action it deems necessary before it clears the fault.
6. The SEP sends a CMD_RECOV_ACK command message to clear the fault.
7. The KM deasserts the recoverable error signal.
8. The KM resumes normal operation, handling all valid commands.
An unrecoverable error signals a condition that puts the KM into a permanent halt state. It has
the following flow:
1. The KM shreds all crypto engine key sideload registers.
2. The KM flushes the mailbox.
3. The KM sends the RESP_UNRECOVERABLE_FAULT response message to the SEP
with a code indicating the cause of the fault.
4. The KM shreds the entire SRAM using one pass of a Weyl sequence followed by two
passes of a xoshiro128++ PRNG.
5. The KM CPU permanently halts, which sets the unrecoverable error signal to interrupt
the SEP.
6. The SEP must hard-reset the KM to bring it back through the boot sequence.

## Bootloader

### Boot Sequence

The ROM bootloader will perform the following sequence of steps:

1. Enable all fault and error related interrupts.
2. Initialize the DRBG sampler.
3. Seed the firmware PRNG from the DRBG.
4. If the SRAM scrambler is not enabled.
    a. Write the SRAM scrambler key register SHRED_ITER+1 number of times with
       random words sourced from the DRBG.
    b. Using inline assembly, enable & lock the SRAM scrambler then restart CPU
       execution (jump to address 0).
5. Initialize the KPV.
    a. Initialize the KPV scrambler.
    b. Enable the KPV scrambler.
    c. Lock the KPV scrambler.


```
d. Shred all KPV slots.
```
6. Initialize the HMAC engine key interface.
    a. Shred the HMAC key registers.
7. Initialize the KMAC engine key interface.
    a. Shred the KMAC key registers.
8. Initialize the AES engine key interface.
    a. Shred the AES key registers.
9. Initialize the OTBN engine key interface.
    a. Shred the OTBN key registers.
10. Initialize the message buffers.
11. Clear and disable the outgoing mailbox FIFO interrupt.
12. Clear and enable the incoming mailbox FIFO interrupt.
13. Send a RESP_KM_READY response message to the SEP.
14. Finally, enter the main event loop.

## Message Buffers

The message receive and transmit buffers are fixed size SRAM-based circular buffers that store
message frames that have been read from or are ready to be written to the mailbox FIFOs. The
buffers must also keep track of pointers to the start and end of each message frame stored in
the buffer. Since the KM firmware does not have a heap, the KM must allocate global or
dynamic memory for message receive and transmit buffers. Since the maximum message
payload length is 255 words, each message buffer is 257 words in size (1 header word + 255
payload words + 1 CRC-32C word) and a single message buffer can store multiple messages.
All access to the message buffers must go through a well defined interface. The interface must
define functions for initializing, reading, writing, and getting status from a message buffer. The
interface must support reading and writing partial message frames, and it must prevent buffer
overflow and underflow. In order to avoid synchronization issues, the mailbox IRQs must be
disabled during buffer read/write operations and then returned to their previous state once
complete.

## Pseudorandom Generators

### Firmware PRNG

The KM firmware shall use the xoshiro128++ algorithm as its general-purpose pseudorandom
number generator. The PRNG is used for generating shred data and seeding
pseudorandom-order operations. It must not be used for generating cryptographic key material;
all key material must be derived from a master key or sourced directly from the DRBG.


#### State

The PRNG maintains a 128-bit internal state consisting of four 32-bit words, denoted s[0], s[1],
s[2], and s[3].

#### Seed

The PRNG is seeded by reading four consecutive words from the DRBG and assigning them to
s[0] through s[3]. An all-zero state is an absorbing fixed point of xoshiro128++ and must be
avoided. If all four seed words are zero, s[0] must be set to 1 before use. The PRNG is
reseeded on-demand from the DRBG via a function call.

#### Implementation Constraints

Since the PRNG is used for SRAM shredding, the PRNG routines and the PRNG state must
reside entirely in ROM code and CPU registers, respectively. The SRAM shred routine and all
PRNG routines must be implemented using inline functions and assembly. The PRNG state,
write pointer, loop bound, and temporaries must be held in CPU registers for the duration of the
SRAM shred loop. The PRNG state must not be stored in SRAM at any point during SRAM
shredding.

### Register Access Shuffling

All operations requiring pseudorandom-order register access must use the Fisher-Yates shuffle
to generate the access order. This includes all shredding and key write operations on the KPV,
crypto engine key registers, SRAM scrambler key, and KPV scrambler key. The shuffle operates
on an index array initialized to [0, 1, ..., n-1] and produces a uniformly random permutation.

#### Index Generation

The random index j in the range [0, bound) is generated using bit-masked rejection sampling
from a PRNG-fed bit pool. This avoids division and eliminates modulo bias.
The bit pool is a 32-bit register refilled from the PRNG whenever insufficient bits remain. Bits are
consumed from LSB to MSB.

## Interrupt Handlers

The PicoRV32 has a very basic interrupt controller that does not support hardware interrupt
priority or nested interrupts. Due to these limitations, interrupt priority must be managed by
firmware and all interrupt handlers must be kept as short as possible to minimize interrupt
response latency. The main ISR must process all currently active interrupts within a single call.

### Interrupt Priority

Interrupt priority must be handled by firmware within the PicoRV32’s main interrupt service
routine (ISR). Any new interrupts that arrive while the PicoRV32 ISR is executing will trigger a


pending interrupt signal that will then re-execute the main ISR after it returns. Since higher
priority interrupts cannot preempt lower priority interrupts, short and deterministic ISR execution
time is important.
The ISR should prioritize interrupts in the following order, from highest priority to lowest priority:

1. EBREAK / Illegal instruction
2. Bus error or misaligned access
3. Wipe state
4. ROM parity error
5. SRAM parity error
6. ROM write error
7. SRAM write lock error
8. AXI DECERR
9. AXI SLVERR
10. DRBG error
11. Mailbox
12. Spurious IRQ (catch-all for unrecognised sources)

### EBREAK / Illegal Instruction

The EBREAK / illegal instruction interrupt handler distinguishes the faulting instruction by
reading the opcode at the saved PC. An EBREAK instruction triggers an unrecoverable fault
with the `ebreak` fault code. Any other illegal instruction triggers an unrecoverable fault with the
`illegal_insn` fault code. The interrupt status bit is not cleared.

### Bus Error or Misaligned Access

The bus error or misaligned access interrupt handler will trigger an unrecoverable fault. The
interrupt status bit is not cleared. This behavior may change in future firmware revisions.

### Wipe State

The wipe state interrupt handler will purge keys from all crypto engines, trigger an
unrecoverable fault, and finally shred the entire SRAM. Hardware will automatically clear the
KPV, so firmware can skip it. The interrupt status bit is not cleared.
SRAM shredding is done last, and must be implemented entirely within assembly code that
does not perform any SRAM read operations. Any operations after SRAM shredding must also
be implemented in assembly since all program state (stack, etc.) has been destroyed.
Note: To ensure the wipe state process does not hang, the PRNG is _never_ reseeded from the
DRBG during any shred operation in the wipe state flow. This diverges from typical shred
operations that normally reseed the PRNG from the DRBG.


### ROM Parity Error

The ROM parity error interrupt handler will trigger an unrecoverable fault. The interrupt status bit
is not cleared. This behavior may change in future firmware revisions.

### SRAM Parity Error

The SRAM parity error interrupt handler will trigger an unrecoverable fault. The interrupt status
bit is not cleared. This behavior may change in future firmware revisions.

### ROM Write Error

The ROM write error interrupt handler will trigger an unrecoverable fault. The interrupt status bit
is not cleared. This behavior may change in future firmware revisions.

### SRAM Write Lock Error

The SRAM write lock error interrupt handler will trigger an unrecoverable fault. The interrupt
status bit is not cleared. This behavior may change in future firmware revisions.

### AXI DECERR

The AXI DECERR interrupt handler will trigger an unrecoverable fault. The interrupt status bit is
not cleared. This behavior may change in future firmware revisions.

### AXI SLVERR

The AXI SLVERR interrupt handler will trigger an unrecoverable fault. The interrupt status bit is
not cleared. This behavior may change in future firmware revisions.

### DRBG Error

The DRBG error interrupt handler will trigger an unrecoverable fault. The interrupt status bit is
not cleared. This behavior may change in future firmware revisions.

### Mailbox

The mailbox interrupt handler transfers message frames between the mailbox FIFOs and the
message buffers. It also handles any other interrupts raised by the mailbox due to error or status
conditions. It does not check the properties of incoming messages or construct the headers of
outgoing messages, as this is the job of the message handlers.
The mailbox interrupt handler has the following execution flow:

1. Check for and handle any mailbox error interrupts.
    a. If the OUTBOUND_OVERFLOW interrupt is enabled and asserted.
       i. Set the RECOVERABLE_ERR register of the KMCSR.
ii. Flush _all_ message buffers and mailbox FIFOs.


```
iii. Message sequence numbers are reset to zero.
iv. A RESP_RECOVERABLE_FAULT response is sent to the SEP with a
mbox_overflow fault code through the mailbox FIFOs directly. The
message transmit buffer is bypassed.
v. Clear the OUTBOUND_OVERFLOW interrupt status bit.
vi. Enable the incoming mailbox FIFO interrupt if necessary.
vii. Return immediately.
b. If the INBOUND_UNDERFLOW interrupt status is enabled and asserted.
i. Set the RECOVERABLE_ERR register of the KMCSR.
ii. Flush all message buffers and mailbox FIFOs.
iii. Message sequence numbers are reset to zero.
iv. A RESP_RECOVERABLE_FAULT response is sent to the SEP with a
mbox_underflow fault code through the mailbox FIFOs directly. The
message transmit buffer is bypassed.
v. Clear the INBOUND_UNDERFLOW interrupt status bit.
vi. Enable the incoming mailbox FIFO interrupt if necessary.
vii. Return immediately.
c. If the FLUSHED_BY_SEP interrupt status is enabled and asserted.
i. Set the RECOVERABLE_ERR register of the KMCSR.
ii. Flush all message buffers, but do not flush the mailbox FIFOs.
iii. Message sequence numbers are reset to zero.
iv. A RESP_RECOVERABLE_FAULT response is sent to the SEP with a
flushed_by_sep fault code through the mailbox FIFOs directly. The
message transmit buffer is bypassed.
v. Clear the FLUSHED_BY_SEP interrupt status bit.
vi. Enable the incoming mailbox FIFO interrupt if necessary.
vii. Return immediately.
```
2. Check for any pending words in the message transmit buffer.
    a. Calculate the minimum between the number of words currently in the message
       transmit buffer, and the available capacity of the outgoing mailbox FIFO.
    b. Transfer this determined count of words from the message transmit buffer into
       the outgoing mailbox FIFO.
    c. Set the separator bit in the FIFO before writing the final message frame word.
    d. After words are transferred, if the message transmit buffer is empty, disable the
       outgoing mailbox FIFO interrupt.
3. Check for any pending words in the incoming mailbox FIFO.
    a. Calculate the minimum between the number of words currently in the incoming
       mailbox FIFO, and the available capacity of the message receive buffer.
    b. Transfer this determined count of words from the incoming mailbox FIFO into the
       message receive buffer.
    c. Check the separator bit after each word is moved from the FIFO to the buffer.
       The separator bit is aligned with the last word of each message frame.
    d. Record pointers for the start/end words of each message frame.


```
e. After the words are transferred, if the message receive buffer is full, disable the
incoming mailbox FIFO interrupt.
```
### Spurious IRQ

After all known IRQ sources have been serviced, the ISR checks for any unrecognised bits in
the IRQ mask. If any are set, the ISR triggers an unrecoverable fault with the _spurious_irq_ fault
code. This ensures security-critical firmware never silently ignores unexpected interrupt sources.

## Incoming Message Handler

The incoming message handler parses incoming message frames from the message receive
buffer in SRAM, checks their properties, and calls the associated command handler. The
incoming message handler is called as part of the main task loop of the KM system firmware,
and it processes a single message frame per-call to the function. It must ensure that invalid or
malformed messages are not processed by the KM. It must use functions provided by the
message buffer interface and may not manipulate the message buffer directly. Partial message
frames, i.e. message frames that have not been fully received with a message separator
indicator, are skipped.
The incoming message handler has no arguments. It has the following execution flow:

1. If the message receive buffer is full AND it only contains a single partial message frame.
    a. Set the RECOVERABLE_ERR register of the KMCSR.
    b. Flush _all_ message buffers and mailbox FIFOs.
    c. A RESP_RECOVERABLE_FAULT response is sent to the SEP with an
       _rx_buff_oflow_ fault code.
    d. Enable the incoming mailbox FIFO interrupt if necessary.
    e. Return immediately.
2. If the message receive buffer is empty OR it only contains a single partial message
    frame.
       a. Enable the incoming mailbox FIFO interrupt if necessary.
       b. Return immediately.
3. Acquire the next fully valid message frame from the message receive buffer.
4. If the message header CRC-8/ROHC is invalid.
    a. A RESP_CMD response is sent back to the SEP with a _header_crc_ return code
       and the handler returns.
5. If the sequence number is not the next expected value.
    a. A RESP_CMD response is sent back to the SEP with a _cmd_noseq_ return code
       and the handler returns.
6. If the command ID is not a valid command.
    a. A RESP_CMD response is sent back to the SEP with an _invalid_cmd_ return code
       and the handler returns.


7. If the payload length is not consistent with the message frame length counted during
    transfer from the incoming mailbox FIFO to the message receive buffer.
       a. A RESP_CMD response is sent back to the SEP with an _invalid_len_ return code
          and the handler returns.
8. If the message has a payload and the payload CRC-32C is invalid.
    a. A RESP_CMD response is sent back to the SEP with a _payload_crc_ return code
       and the handler returns.
9. If the RECOVERABLE_ERR bit is set in the KMCSR.
    a. Only allow the following commands.
       i. CMD_HW_VER
ii. CMD_ROM_VER
iii. CMD_SRAM_VER
iv. CMD_STAT
v. CMD_RECOV_ACK
    b. A RESP_CMD response is sent back to the SEP with the return code and return
       argument from the command handler.
    c. Respond to all other commands with a RESP_CMD response of _failure_ and do
       not execute the command.
10. If the RECOVERABLE_ERR bit is _not_ set in the KMCSR.
    a. Call the command handler corresponding to the command ID and include the
       following arguments.
          i. The length of the message payload.
ii. A structure containing the message payload.
    b. A RESP_CMD response is sent back to the SEP with the return code and return
       argument from the command handler.
11. Enable the incoming mailbox FIFO interrupt if necessary.
12. Return.

## Outgoing Message Handler

The outgoing message handler constructs a message from a given response payload and
writes it into the message transmit buffer in SRAM. The outgoing message handler is called
from command handlers and other system functions to send responses to the SEP. It must use
functions provided by the message buffer interface and may not manipulate the message buffer
directly.
The outgoing message handler takes a response ID and a payload as arguments. It has the
following execution flow:

1. The next message sequence number is inserted into the message header.
2. The response ID is inserted into the message header.
3. The payload length is calculated and inserted into the message header.
4. The message header CRC-8/ROHC is calculated and inserted into the header.


5. If the payload length is nonzero, the payload CRC-32C is calculated and appended to
    the message.
6. The entire message frame is written into the message transmit buffer. If the buffer is full,
    enable the outgoing mailbox FIFO interrupt and block (spin-wait) until enough free space
    is available to write the entire frame. Further execution depends on the SEP draining the
    outgoing mailbox FIFO.
7. Enable the outgoing mailbox FIFO interrupt if necessary.
8. Return.

## Command Handlers

Each supported KM command has an associated command handler function. They are called
by the incoming message handler, validate the arguments provided in the command message,
and then call the appropriate system or key management function. If a command handler
encounters an error it will return with an error code and an optional return argument. This
includes errors encountered during payload parsing. Otherwise, if there are no errors it will
return a _success_ code and provide an optional return argument.

## DRBG Driver

### Initialize DRBG

Configures the DRBG and waits for it to be ready.
It has the following execution flow:

1. Wait for DRBG_READY in the STATUS register.

### Get Status

Returns the value of the STATUS register.

### Get Word

Returns a single random word from the DRBG.

### Get Block

Gets a block of random words from the DRBG, given a number of words to read.
It has the following execution flow:

1. Enable DRBG prefetch.
2. Read the given number of random words.
3. Disable DRBG prefetch.


4. Return the random words.

## KPV Driver

These functions provide firmware abstractions for the KPV.

### Initialize Scrambler

If unlocked, writes the KPV scrambler key register SHRED_ITER+1 number of times with
random words sourced from the DRBG.

### Scrambler Enable

Enables the KPV scrambler.

### Scrambler Lock

Locks the KPV scrambler.

### Shred KPV

Purges KPV key data registers with random data from the PRNG and clears the control
registers.
It has the following execution flow:

1. Clears all slot control registers, skipping any registers that are write-locked.
2. Writes random data sourced from the PRNG to all key data registers.
    a. Reseeds the PRNG from the DRBG.
    b. Writes the registers in a pseudorandom order that is seeded from the PRNG,
       skipping any registers that are write-locked.
    c. Performs this entire sequence a total of SHRED_ITER+1 times.

### Shred Slot

Purges a given key slot with random data from the PRNG and clears its control registers. It has
the following execution flow:

1. Clears the slot control register, returning with an error if the slot is write-locked.
2. Writes random data sourced from the PRNG to the given slot.
    a. Reseeds the PRNG from the DRBG.
    b. Writes the slot words in a pseudorandom order that is seeded from the PRNG.
    c. Performs this entire sequence a total of SHRED_ITER+1 times.


### Write Key

Writes key data and control information to a range of slot indexes, given the following:
● A base slot index for storing the key
● The key data
● The key length (in 32-bit words)
● Valid crypto engine destinations for the key
It has the following execution flow:

1. If any required key slots are write locked.
    a. The function returns an error. No key data is written to the KPV.
2. The key is written to the KPV, starting with the given base slot index.
    a. The total number of required key slots (EXTEND+1) is equal to (KEY_LENGTH - 1)
       / 16 + 1.
    b. The 4-bit LAST_DWORD field of the final key slot is equal to (KEY_LENGTH % 16
       == 0)? 15 : ((KEY_LENGTH % 16) - 1).
    c. The LAST_DWORD field of all other required key slots must be set to 15.
    d. The given bitmask of valid crypto engine destinations is written to all required key
       slots.

### Read Key

Reads key data and control information, given a base slot index. The function returns the
following:
● The key data
● The key length (in 32-bit words)
● Valid crypto engine destinations for the key
It has the following execution flow:

1. If any required key slots are read locked.
    a. The function returns an error. No key data is returned from the KPV.
2. The key is read from the KPV, starting with the given base key slot.
    a. The total number of required key slots is equal to EXTEND + 1 , where EXTEND
       comes from the base key slot.
    b. The number of words in the final key slot is equal to LAST_DWORD + 1 , where
       LAST_DWORD comes from the final key slot.
    c. LAST_DWORD for all other required key slots _must_ be equal to 15.
    d. The total key length (in 32-bit words) is thus 16 * EXTEND + (LAST_DWORD + 1),
       where EXTEND comes from the base key slot and LAST_DWORD comes from
       the final key slot.
    e. The bitmask of valid crypto engine destinations is derived from the base key slot.


### Write-Lock Key

Write-locks the key at a given base slot index. The function uses the EXTEND field of the base
slot control register to lock all associated key slots.

### Read-Lock Key

Read-locks the key at a given base slot index. The function uses the EXTEND field of the base
slot control register to lock all associated key slots.

## HMAC Driver

The HMAC driver manages key material for the HMAC crypto engine.

### Shred Key

Purges the sideload key share registers with random data from the PRNG.
It has the following execution flow:

1. Clears the key valid bit.
2. Writes random data sourced from the PRNG to both key share registers.
    a. Reseeds the PRNG from the DRBG.
    b. Writes the registers with random data from the PRNG across both key share
       registers in a pseudorandom order that is seeded from the PRNG. Write order
       should be randomly interleaved across both key share registers to enhance
       obfuscation.
    c. Performs this entire sequence a total of SHRED_ITER+1 times.

### Write Key

Writes key data to the sideload key share registers in a pseudorandom order that is seeded
from the PRNG. The two key share registers XOR together within the accelerator to produce the
real key.
It has the following execution flow:

1. Pads the given key, KEY, with random data from the DRBG up to the full length of one of
    the sideload key share registers.
2. Collect a random array of data, RAND, from the DRBG the same size as the padded key.
3. For each word, i: KEY_RAND[i] = XOR(KEY[i], RAND[i])
4. Writes RAND to KEY_SHARE0 and KEY_RAND to KEY_SHARE1 in a pseudorandom order that is
    seeded from the PRNG. Write order should be randomly interleaved across both key
    share registers to enhance obfuscation.
5. Sets the key valid bit.


## KMAC Driver

The KMAC driver manages key material for the KMAC crypto engine.

### Shred Key

Purges the sideload key share registers with random data from the PRNG.
It has the same execution flow as the HMAC shred key driver.

### Write Key

Writes key data to the sideload key share registers in a pseudorandom order that is seeded
from the PRNG. The two key share registers XOR together within the accelerator to produce the
real key.
It has the same execution flow as the HMAC write key driver.

## AES Driver

The AES driver manages key material for the AES crypto engine.

### Shred Key

Purges the sideload key share registers with random data from the PRNG.
It has the same execution flow as the HMAC shred key driver.

### Write Key

Writes key data to the sideload key share registers in a pseudorandom order that is seeded
from the PRNG. The two key share registers XOR together within the accelerator to produce the
real key.
It has the same execution flow as the HMAC write key driver.

## OTBN Driver

The OTBN driver manages key material for the OTBN crypto engine.

### Shred Key

Purges the sideload key share registers with random data from the PRNG.
It has the same execution flow as the HMAC shred key driver.


### Write Key

Writes key data to the sideload key share registers in a pseudorandom order that is seeded
from the PRNG. The two key share registers XOR together within the accelerator to produce the
real key.
It has the same execution flow as the HMAC write key driver.

## CRC Function Library

The CRC function library contains routines for CRC computations.

### CRC-8/ROHC

Compute the CRC-8/ROHC for a given block of data.
Parameters:
● **Width:** 8
● **Poly:** 0x07
● **Init:** 0xFF
● **RefIn:** true
● **RefOut:** true
● **XorOut:** 0x00
● **Check ("123456789"):** 0xD0
The CRC-8/ROHC is computed over the input bytes in little endian order, from least significant
byte to most significant byte.

### CRC-32C

Compute the CRC-32C (Castagnoli) for a given block of data.
Parameters:
● **Width:** 32
● **Poly (normal):** 0x1EDC6F41
● **Poly (reflected):** 0x82F63B78
● **Init:** 0xFFFFFFFF
● **RefIn:** true
● **RefOut:** true
● **XorOut:** 0xFFFFFFFF
● **Check ("123456789"):** 0xE3069283
The CRC-32C is computed over the input bytes in little endian order, from least significant byte
to most significant byte.


## Key Registry

KM firmware maintains a mapping between key handles and key slots in the KPV, called the key
registry. Key handles are unique 8-bit numbers that act as aliases to key slots in the KPV.
Handle 0x00 is reserved for “null”, i.e. no handle is assigned. Key handles are simply
incremented as they are allocated in the registry. Since there are only 32 key slots, key handle
assignment can technically never accidentally roll over to 0. However, to be safe, the key
registry should still check for key exhaustion before allocating new keys. Key handles cannot be
reused, and are permanently invalidated when they are destroyed.
There are two data structures in the key registry:
● A mapping from key handles to a base slot index, as well as a CRC-32C value for the
associated key.
● A mapping from slot indexes to a key handle. A key handle value of NULL (0x00)
indicates the slot does not have an associated key handle.
All access to the key registry must go through a well defined interface. The following functions
manipulate the key registry.

### Get Key Handle

Returns the registered key handle, given a key slot index. The given key slot index does not
need to be the base slot for the key. Returns an error if the key slot does not have an associated
handle, i.e. the handle is NULL (0x00).

### Get Key Slot

Returns the registered base key slot index, given a key handle. Returns an error if the key
handle does not exist or has been invalidated.

### Get Key CRC

Returns the registered CRC-32C for a key, given a key handle. Returns an error if the key
handle does not exist or has been invalidated.

### Generate Handle

Returns and registers a unique 8-bit key handle, given the following:
● The base slot index for a key
● A CRC-32C of the key data
It has the following execution flow:

1. Returns with an error if any of the key’s slots are already registered.


2. Selects a unique 8-bit key handle not invalidated or currently assigned to any existing
    key slot.
       a. Walk through the registry and find the next unassigned key handle.
       b. Return with an error if no key handles are available.
3. Updates the registry with the generated key handle and associated CRC-32C.

### Destroy Handle

Invalidates a given key handle in the registry. The key handle may not be used again. Returns
with an error if the given key handle does not exist.

## Key Management

These functions provide high-level key management operations. In general, they are called by
command handler functions.

### Check Key

Checks the integrity of a stored key, given a key handle. Returns an error if the key handle does
not exist or the CRC-32C is invalid. Triggers a recoverable fault if the CRC-32C is invalid.
It has the following execution flow:

1. Get the key slot and CRC-32C for the given key handle, returning with an error if invalid.
2. Calculate the CRC-32C for the key data located in the key slot.
3. If the calculated CRC-32C does not match the CRC-32C stored in the key registry.
    a. Set the RECOVERABLE_ERR register of the KMCSR.
    b. A RESP_RECOVERABLE_FAULT response is sent to the SEP with a
       _key_slot_crc_ fault code.
    c. Return with an error.

### Allocate KPVLP Slot

Allocates slots to the SEP for key entry via the KPVLP, given a number of slots requested.
It has the following execution flow:

1. Finds a set of unoccupied consecutive key slots, returning with an error if none are
    available.
       a. The base key slot must be selected randomly using data from the DRBG.
       b. Slots allocated to the SEP must be avoided.
       c. Selected slots must not be write-locked or read-locked.
       d. Selected slots must not be associated with an existing key handle.
2. Shreds all allocated slots.
3. Sets the UNLOCK_SEP bit on all allocated slots.


### Generate Key

Generates a random key, given the following:
● The size of the key in multiples of 32-bit words
● Valid crypto engine destinations for the key
It has the following execution flow:

1. Finds a set of unoccupied consecutive key slots, returning with an error if none are
    available.
       a. The base key slot must be selected randomly using data from the DRBG.
       b. Slots allocated to the SEP must be avoided.
       c. Selected slots must not be write-locked or read-locked.
       d. Selected slots must not be associated with an existing key handle.
2. Shreds all slots associated with the key.
3. Collects random data from the DRBG for the key data.
4. Writes the key to the KPV.
5. Write-locks all slots associated with the key.
6. Generates and returns a handle for the key.

### Register KPVLP Key

Registers a key already loaded into the KPV via the KPVLP, given the following:
● The base slot index of the loaded key
● The size of the key in multiples of 32-bit words
● Valid crypto engine destinations for the key
● A CRC-32C for the key data
It has the following execution flow:

1. Makes a temporary local copy of the key data and control registers for the associated
    slots.
2. Checks that none of the slots associated with the key are write-locked or read-locked,
    returning with an error if any are invalid.
3. Checks that all slots associated with the key are allocated to the SEP, returning with an
    error if any are invalid.
4. Checks that none of the key slots already have an associated key handle.
5. Checks that the provided key parameters are consistent with the key control registers for
    each associated slot, returning with an error if any are invalid.
6. Checks that the CRC-32C of the key data loaded into the associated slots is consistent
    with the given CRC-32C, returning with an error if invalid.
7. Write-locks all slots associated with the key. SEP write permission is revoked once this
    bit is set.


8. Checks that the key control and data registers remain identical to the temporary local
    copy made at the beginning, returning with an error if invalid.
9. Generates and returns a handle for the key.

### Revoke Key

Revokes a key, given a key handle.
It has the following execution flow:

1. Gets the base slot index associated with the key handle, returning with an error if invalid.
2. Write-locks and read-locks all slots associated with the key.
3. Destroys the given key handle.
4. Returns success.

### Transfer Key

Transfers a key from the KPV to one or more crypto engines, given the following:
● The handle for the key
● A bitfield of crypto engine destinations for the key
It has the following execution flow:

1. Gets the base slot index associated with the key handle, returning with an error if invalid.
2. Checks the integrity of the key associated with the given handle, returning with an error if
    invalid.
3. Checks that **_all_** requested crypto engine destinations are allowed, returning with an error
    if any are invalid.
4. Transfers the key to all requested crypto engines.
5. Returns success.

## Main Event Loop

After the bootloader initializes the KM, the main loop of the KM continuously monitors the
incoming message buffer for commands. When the incoming message buffer is empty, the main
loop should wait for an interrupt. All other tasks are handled by the main ISR.
The KM processes commands strictly in the order that they are received.

### Fault Handling

When the KM encounters an unrecoverable fault, it will flush the mailbox FIFOs and then write a
RESP_UNRECOVERABLE_FAULT message directly to the outgoing mailbox FIFO. The


message transmit buffer is entirely bypassed for this operation. Finally, the PicoRV32 will trap so
that it halts execution. At this point, the SEP must hard-reset the KM.
When the KM encounters a recoverable fault, it should set the RECOVERABLE_ERR register
bit in the KMCSR. The intent is that the KM should halt critical services until the SEP has had a
chance to acknowledge the fault and take any desired action. If this bit is set, the KM should not
process any key management commands until the SEP formally acknowledges the fault with a
CMD_RECOV_ACK command. The RESP_RECOVERABLE_FAULT message is always sent
back to the SEP in addition to any error messages returned from any command that may have
triggered the fault.

## Firmware Load

##### THIS SECTION IS WIP

### Loading Sequence

Immediately after receiving the CMD_FIRM_LOAD command, the KM will begin transferring the
raw words of the next mailbox frame directly into the SRAM, starting at the provided base
address. Since writing the image to SRAM is a destructive operation, this code must be written
in assembly and must restrict its working state to local CPU registers. It will follow the following
sequence:

4. Do the following in a loop:
    a. Read a word from the mailbox.
    b. Check the separator status bit.
    c. If the separator bit is set, the word that was just read is the CRC for the image.
       Break from the loop.
    d. Write the word into the next SRAM location, where the first location starts at the
       provided IMAGE_BASE_ADDRESS.
    e. Read the word back from SRAM and compute the next CRC state from this
       value.
5. Compare the word that was just read from the mailbox with the computed CRC state. If
    they don’t match, trigger an unrecoverable fault.
6. Jump to the provided JUMP_ADDRESS.


