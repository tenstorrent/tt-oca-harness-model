from ctypes import Structure, Union, c_uint8, c_uint16, c_uint32, c_uint64

KM_CSR_REG_MAP_BASE_ADDR = 0x00000000
KM_CSR_REG_MAP_SIZE = 0x00000200
VERSION_REG_OFFSET = 0x00000000
VERSION_REG_ADDR = 0x00000000
CTRL_REG_OFFSET = 0x00000004
CTRL_REG_ADDR = 0x00000004
SOFT_RST_CODE_REG_OFFSET = 0x00000008
SOFT_RST_CODE_REG_ADDR = 0x00000008
IRQ_STATUS_REG_OFFSET = 0x0000000C
IRQ_STATUS_REG_ADDR = 0x0000000C
IRQ_ENABLE_REG_OFFSET = 0x00000010
IRQ_ENABLE_REG_ADDR = 0x00000010
SCRAMBLER_KEY_REG_OFFSET = 0x00000014
SCRAMBLER_KEY_REG_ADDR = 0x00000014
SCRAMBLER_CTRL_REG_OFFSET = 0x00000018
SCRAMBLER_CTRL_REG_ADDR = 0x00000018
SRAM_LOCK_REG_OFFSET = 0x0000001C
SRAM_LOCK_REG_ADDR = 0x0000001C
IRQ_SET_REG_OFFSET = 0x00000020
IRQ_SET_REG_ADDR = 0x00000020
SRAM_WRITE_LOCK_VIOLATION_REG_OFFSET = 0x00000024
SRAM_WRITE_LOCK_VIOLATION_REG_ADDR = 0x00000024
RECOVERABLE_ERR_REG_OFFSET = 0x00000028
RECOVERABLE_ERR_REG_ADDR = 0x00000028
OTP_LIFE_CYCLE_REG_OFFSET = 0x00000030
OTP_LIFE_CYCLE_REG_ADDR = 0x00000030
OTP_DEMOTION_STATE_REG_OFFSET = 0x00000034
OTP_DEMOTION_STATE_REG_ADDR = 0x00000034
OTP_CHIPLET_UID_0_REG_OFFSET = 0x00000038
OTP_CHIPLET_UID_0_REG_ADDR = 0x00000038
OTP_CHIPLET_UID_1_REG_OFFSET = 0x0000003C
OTP_CHIPLET_UID_1_REG_ADDR = 0x0000003C
OTP_CHIPLET_UID_2_REG_OFFSET = 0x00000040
OTP_CHIPLET_UID_2_REG_ADDR = 0x00000040
OTP_CHIPLET_UID_3_REG_OFFSET = 0x00000044
OTP_CHIPLET_UID_3_REG_ADDR = 0x00000044
OTP_CHIPLET_UID_4_REG_OFFSET = 0x00000048
OTP_CHIPLET_UID_4_REG_ADDR = 0x00000048
OTP_CHIPLET_UID_5_REG_OFFSET = 0x0000004C
OTP_CHIPLET_UID_5_REG_ADDR = 0x0000004C
OTP_CHIPLET_UID_6_REG_OFFSET = 0x00000050
OTP_CHIPLET_UID_6_REG_ADDR = 0x00000050
OTP_CHIPLET_UID_7_REG_OFFSET = 0x00000054
OTP_CHIPLET_UID_7_REG_ADDR = 0x00000054
OTP_CHIPLET_UID_8_REG_OFFSET = 0x00000058
OTP_CHIPLET_UID_8_REG_ADDR = 0x00000058
OTP_CHIPLET_UID_9_REG_OFFSET = 0x0000005C
OTP_CHIPLET_UID_9_REG_ADDR = 0x0000005C
OTP_CHIPLET_UID_10_REG_OFFSET = 0x00000060
OTP_CHIPLET_UID_10_REG_ADDR = 0x00000060
OTP_CHIPLET_UID_11_REG_OFFSET = 0x00000064
OTP_CHIPLET_UID_11_REG_ADDR = 0x00000064
OTP_CHIPLET_UID_12_REG_OFFSET = 0x00000068
OTP_CHIPLET_UID_12_REG_ADDR = 0x00000068
OTP_CHIPLET_UID_13_REG_OFFSET = 0x0000006C
OTP_CHIPLET_UID_13_REG_ADDR = 0x0000006C
OTP_CHIPLET_UID_14_REG_OFFSET = 0x00000070
OTP_CHIPLET_UID_14_REG_ADDR = 0x00000070
OTP_CHIPLET_UID_15_REG_OFFSET = 0x00000074
OTP_CHIPLET_UID_15_REG_ADDR = 0x00000074
OTP_CHIPLET_UID_16_REG_OFFSET = 0x00000078
OTP_CHIPLET_UID_16_REG_ADDR = 0x00000078
OTP_CHIPLET_UID_17_REG_OFFSET = 0x0000007C
OTP_CHIPLET_UID_17_REG_ADDR = 0x0000007C
OTP_CHIPLET_UID_18_REG_OFFSET = 0x00000080
OTP_CHIPLET_UID_18_REG_ADDR = 0x00000080
OTP_CHIPLET_UID_19_REG_OFFSET = 0x00000084
OTP_CHIPLET_UID_19_REG_ADDR = 0x00000084
OTP_CHIPLET_UID_20_REG_OFFSET = 0x00000088
OTP_CHIPLET_UID_20_REG_ADDR = 0x00000088
OTP_CHIPLET_UID_21_REG_OFFSET = 0x0000008C
OTP_CHIPLET_UID_21_REG_ADDR = 0x0000008C
OTP_CHIPLET_UID_22_REG_OFFSET = 0x00000090
OTP_CHIPLET_UID_22_REG_ADDR = 0x00000090
OTP_CHIPLET_UID_23_REG_OFFSET = 0x00000094
OTP_CHIPLET_UID_23_REG_ADDR = 0x00000094
OTP_CHIPLET_UID_24_REG_OFFSET = 0x00000098
OTP_CHIPLET_UID_24_REG_ADDR = 0x00000098
OTP_CHIPLET_UID_25_REG_OFFSET = 0x0000009C
OTP_CHIPLET_UID_25_REG_ADDR = 0x0000009C
OTP_CHIPLET_UID_26_REG_OFFSET = 0x000000A0
OTP_CHIPLET_UID_26_REG_ADDR = 0x000000A0
OTP_CHIPLET_UID_27_REG_OFFSET = 0x000000A4
OTP_CHIPLET_UID_27_REG_ADDR = 0x000000A4
OTP_CHIPLET_UID_28_REG_OFFSET = 0x000000A8
OTP_CHIPLET_UID_28_REG_ADDR = 0x000000A8
OTP_CHIPLET_UID_29_REG_OFFSET = 0x000000AC
OTP_CHIPLET_UID_29_REG_ADDR = 0x000000AC
OTP_CHIPLET_UID_30_REG_OFFSET = 0x000000B0
OTP_CHIPLET_UID_30_REG_ADDR = 0x000000B0
OTP_CHIPLET_UID_31_REG_OFFSET = 0x000000B4
OTP_CHIPLET_UID_31_REG_ADDR = 0x000000B4
IRQ_ENTRY_ADDR_REG_OFFSET = 0x000000B8
IRQ_ENTRY_ADDR_REG_ADDR = 0x000000B8
IRQ_ENTRY_LOCK_REG_OFFSET = 0x000000BC
IRQ_ENTRY_LOCK_REG_ADDR = 0x000000BC
VUART_TX_REG_OFFSET = 0x00000100
VUART_TX_REG_ADDR = 0x00000100
VUART_RX_REG_OFFSET = 0x00000104
VUART_RX_REG_ADDR = 0x00000104
VUART_STATUS_REG_OFFSET = 0x00000108
VUART_STATUS_REG_ADDR = 0x00000108
TB_RESULT_REG_OFFSET = 0x00000110
TB_RESULT_REG_ADDR = 0x00000110
TB_SIGNATURE_REG_OFFSET = 0x00000114
TB_SIGNATURE_REG_ADDR = 0x00000114
TB_ERRCODE_REG_OFFSET = 0x00000118
TB_ERRCODE_REG_ADDR = 0x00000118
TB_SUBTEST_REG_OFFSET = 0x0000011C
TB_SUBTEST_REG_ADDR = 0x0000011C
TB_CMD_REG_OFFSET = 0x00000120
TB_CMD_REG_ADDR = 0x00000120
TB_CMD_ARG_REG_OFFSET = 0x00000124
TB_CMD_ARG_REG_ADDR = 0x00000124
TB_CMD_STATUS_REG_OFFSET = 0x00000128
TB_CMD_STATUS_REG_ADDR = 0x00000128
TB_CMD_RESULT_REG_OFFSET = 0x0000012C
TB_CMD_RESULT_REG_ADDR = 0x0000012C
DEBUG_REG_OFFSET = 0x000001FC
DEBUG_REG_ADDR = 0x000001FC
KM_CSR_VERSION_REG_REG_DEFAULT = 0x00010000
KM_CSR_CTRL_REG_REG_DEFAULT = 0x00000000
KM_CSR_SOFT_RST_CODE_REG_REG_DEFAULT = 0x00000000
KM_CSR_IRQ_STATUS_REG_REG_DEFAULT = 0x00000000
KM_CSR_IRQ_ENABLE_REG_REG_DEFAULT = 0x00000000
KM_CSR_SCRAMBLER_KEY_REG_REG_DEFAULT = 0x00000000
KM_CSR_SCRAMBLER_CTRL_REG_REG_DEFAULT = 0x00000000
KM_CSR_SRAM_LOCK_REG_REG_DEFAULT = 0x00000000
KM_CSR_IRQ_SET_REG_REG_DEFAULT = 0x00000000
KM_CSR_SRAM_WRITE_LOCK_VIOLATION_REG_REG_DEFAULT = 0x00000000
KM_CSR_RECOVERABLE_ERR_REG_REG_DEFAULT = 0x00000000
KM_CSR_OTP_LIFE_CYCLE_REG_REG_DEFAULT = 0x00000000
KM_CSR_OTP_DEMOTION_STATE_REG_REG_DEFAULT = 0x00000000
KM_CSR_OTP_CHIPLET_UID_BYTE_REG_REG_DEFAULT = 0x00000000
KM_CSR_IRQ_ENTRY_ADDR_REG_REG_DEFAULT = 0x00000010
KM_CSR_IRQ_ENTRY_LOCK_REG_REG_DEFAULT = 0x00000000
KM_CSR_VUART_TX_REG_REG_DEFAULT = 0x00000000
KM_CSR_VUART_RX_REG_REG_DEFAULT = 0x00000000
KM_CSR_VUART_STATUS_REG_REG_DEFAULT = 0x00000001
KM_CSR_TB_RESULT_REG_REG_DEFAULT = 0x00000000
KM_CSR_TB_SIGNATURE_REG_REG_DEFAULT = 0x00000000
KM_CSR_TB_ERRCODE_REG_REG_DEFAULT = 0x00000000
KM_CSR_TB_SUBTEST_REG_REG_DEFAULT = 0x00000000
KM_CSR_TB_CMD_REG_REG_DEFAULT = 0x00000000
KM_CSR_TB_CMD_ARG_REG_REG_DEFAULT = 0x00000000
KM_CSR_TB_CMD_STATUS_REG_REG_DEFAULT = 0x00000000
KM_CSR_TB_CMD_RESULT_REG_REG_DEFAULT = 0x00000000
KM_CSR_DEBUG_REG_REG_DEFAULT = 0xCAFEBEEF
class KM_CSR_VERSION_REG_reg_t(Structure):
    _fields_ = [
        ('patch', c_uint32, 8),
        ('minor', c_uint32, 8),
        ('major', c_uint32, 8),
        ('rsvd', c_uint32, 8),
    ]

KM_CSR_VERSION_REG_REG_DEFAULT = 0x00010000

class KM_CSR_VERSION_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_VERSION_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_VERSION_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_VERSION_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_CTRL_REG_reg_t(Structure):
    _fields_ = [
        ('rsvd', c_uint32, 32),
    ]

KM_CSR_CTRL_REG_REG_DEFAULT = 0x00000000

class KM_CSR_CTRL_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_CTRL_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_CTRL_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_CTRL_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_SOFT_RST_CODE_REG_reg_t(Structure):
    _fields_ = [
        ('code', c_uint32, 32),
    ]

KM_CSR_SOFT_RST_CODE_REG_REG_DEFAULT = 0x00000000

class KM_CSR_SOFT_RST_CODE_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_SOFT_RST_CODE_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_SOFT_RST_CODE_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_SOFT_RST_CODE_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_IRQ_STATUS_REG_reg_t(Structure):
    _fields_ = [
        ('rom_parity_err', c_uint32, 1),
        ('sram_parity_err', c_uint32, 1),
        ('rom_write_err', c_uint32, 1),
        ('sram_write_lock_err', c_uint32, 1),
        ('axi_slverr', c_uint32, 1),
        ('axi_decerr', c_uint32, 1),
        ('drbg_err', c_uint32, 1),
        ('wipe_state', c_uint32, 1),
        ('rsvd', c_uint32, 24),
    ]

KM_CSR_IRQ_STATUS_REG_REG_DEFAULT = 0x00000000

class KM_CSR_IRQ_STATUS_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_IRQ_STATUS_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_IRQ_STATUS_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_IRQ_STATUS_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_IRQ_ENABLE_REG_reg_t(Structure):
    _fields_ = [
        ('rom_parity_en', c_uint32, 1),
        ('sram_parity_en', c_uint32, 1),
        ('rom_write_en', c_uint32, 1),
        ('sram_write_lock_en', c_uint32, 1),
        ('axi_slverr_en', c_uint32, 1),
        ('axi_decerr_en', c_uint32, 1),
        ('drbg_err_en', c_uint32, 1),
        ('wipe_state_en', c_uint32, 1),
        ('rsvd', c_uint32, 24),
    ]

KM_CSR_IRQ_ENABLE_REG_REG_DEFAULT = 0x00000000

class KM_CSR_IRQ_ENABLE_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_IRQ_ENABLE_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_IRQ_ENABLE_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_IRQ_ENABLE_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_SCRAMBLER_KEY_REG_reg_t(Structure):
    _fields_ = [
        ('key', c_uint32, 32),
    ]

KM_CSR_SCRAMBLER_KEY_REG_REG_DEFAULT = 0x00000000

class KM_CSR_SCRAMBLER_KEY_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_SCRAMBLER_KEY_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_SCRAMBLER_KEY_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_SCRAMBLER_KEY_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_SCRAMBLER_CTRL_REG_reg_t(Structure):
    _fields_ = [
        ('enable', c_uint32, 1),
        ('lock', c_uint32, 1),
        ('rsvd', c_uint32, 30),
    ]

KM_CSR_SCRAMBLER_CTRL_REG_REG_DEFAULT = 0x00000000

class KM_CSR_SCRAMBLER_CTRL_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_SCRAMBLER_CTRL_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_SCRAMBLER_CTRL_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_SCRAMBLER_CTRL_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_SRAM_LOCK_REG_reg_t(Structure):
    _fields_ = [
        ('lock_bits', c_uint32, 32),
    ]

KM_CSR_SRAM_LOCK_REG_REG_DEFAULT = 0x00000000

class KM_CSR_SRAM_LOCK_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_SRAM_LOCK_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_SRAM_LOCK_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_SRAM_LOCK_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_IRQ_SET_REG_reg_t(Structure):
    _fields_ = [
        ('rom_parity_err_set', c_uint32, 1),
        ('sram_parity_err_set', c_uint32, 1),
        ('rom_write_err_set', c_uint32, 1),
        ('sram_write_lock_err_set', c_uint32, 1),
        ('axi_slverr_set', c_uint32, 1),
        ('axi_decerr_set', c_uint32, 1),
        ('drbg_err_set', c_uint32, 1),
        ('wipe_state_set', c_uint32, 1),
        ('rsvd', c_uint32, 24),
    ]

KM_CSR_IRQ_SET_REG_REG_DEFAULT = 0x00000000

class KM_CSR_IRQ_SET_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_IRQ_SET_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_IRQ_SET_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_IRQ_SET_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_SRAM_WRITE_LOCK_VIOLATION_REG_reg_t(Structure):
    _fields_ = [
        ('violation_bits', c_uint32, 32),
    ]

KM_CSR_SRAM_WRITE_LOCK_VIOLATION_REG_REG_DEFAULT = 0x00000000

class KM_CSR_SRAM_WRITE_LOCK_VIOLATION_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_SRAM_WRITE_LOCK_VIOLATION_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_SRAM_WRITE_LOCK_VIOLATION_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_SRAM_WRITE_LOCK_VIOLATION_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_RECOVERABLE_ERR_REG_reg_t(Structure):
    _fields_ = [
        ('recoverable_err', c_uint32, 1),
        ('rsvd', c_uint32, 31),
    ]

KM_CSR_RECOVERABLE_ERR_REG_REG_DEFAULT = 0x00000000

class KM_CSR_RECOVERABLE_ERR_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_RECOVERABLE_ERR_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_RECOVERABLE_ERR_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_RECOVERABLE_ERR_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_OTP_LIFE_CYCLE_REG_reg_t(Structure):
    _fields_ = [
        ('value', c_uint32, 8),
        ('rsvd', c_uint32, 24),
    ]

KM_CSR_OTP_LIFE_CYCLE_REG_REG_DEFAULT = 0x00000000

class KM_CSR_OTP_LIFE_CYCLE_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_OTP_LIFE_CYCLE_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_OTP_LIFE_CYCLE_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_OTP_LIFE_CYCLE_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_OTP_DEMOTION_STATE_REG_reg_t(Structure):
    _fields_ = [
        ('demote_1_value', c_uint32, 2),
        ('demote_2_value', c_uint32, 2),
        ('rsvd', c_uint32, 28),
    ]

KM_CSR_OTP_DEMOTION_STATE_REG_REG_DEFAULT = 0x00000000

class KM_CSR_OTP_DEMOTION_STATE_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_OTP_DEMOTION_STATE_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_OTP_DEMOTION_STATE_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_OTP_DEMOTION_STATE_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_OTP_CHIPLET_UID_BYTE_REG_reg_t(Structure):
    _fields_ = [
        ('value', c_uint32, 8),
        ('rsvd', c_uint32, 24),
    ]

KM_CSR_OTP_CHIPLET_UID_BYTE_REG_REG_DEFAULT = 0x00000000

class KM_CSR_OTP_CHIPLET_UID_BYTE_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_OTP_CHIPLET_UID_BYTE_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_OTP_CHIPLET_UID_BYTE_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_OTP_CHIPLET_UID_BYTE_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_IRQ_ENTRY_ADDR_REG_reg_t(Structure):
    _fields_ = [
        ('addr', c_uint32, 32),
    ]

KM_CSR_IRQ_ENTRY_ADDR_REG_REG_DEFAULT = 0x00000010

class KM_CSR_IRQ_ENTRY_ADDR_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_IRQ_ENTRY_ADDR_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_IRQ_ENTRY_ADDR_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_IRQ_ENTRY_ADDR_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_IRQ_ENTRY_LOCK_REG_reg_t(Structure):
    _fields_ = [
        ('lock', c_uint32, 1),
        ('rsvd', c_uint32, 31),
    ]

KM_CSR_IRQ_ENTRY_LOCK_REG_REG_DEFAULT = 0x00000000

class KM_CSR_IRQ_ENTRY_LOCK_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_IRQ_ENTRY_LOCK_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_IRQ_ENTRY_LOCK_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_IRQ_ENTRY_LOCK_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_VUART_TX_REG_reg_t(Structure):
    _fields_ = [
        ('tx_byte', c_uint32, 8),
        ('rsvd0', c_uint32, 23),
        ('data_valid', c_uint32, 1),
    ]

KM_CSR_VUART_TX_REG_REG_DEFAULT = 0x00000000

class KM_CSR_VUART_TX_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_VUART_TX_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_VUART_TX_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_VUART_TX_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_VUART_RX_REG_reg_t(Structure):
    _fields_ = [
        ('rx_byte', c_uint32, 8),
        ('rsvd0', c_uint32, 23),
        ('data_valid', c_uint32, 1),
    ]

KM_CSR_VUART_RX_REG_REG_DEFAULT = 0x00000000

class KM_CSR_VUART_RX_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_VUART_RX_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_VUART_RX_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_VUART_RX_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_VUART_STATUS_REG_reg_t(Structure):
    _fields_ = [
        ('tx_ready', c_uint32, 1),
        ('rx_valid', c_uint32, 1),
        ('print_enable', c_uint32, 1),
        ('rsvd', c_uint32, 29),
    ]

KM_CSR_VUART_STATUS_REG_REG_DEFAULT = 0x00000001

class KM_CSR_VUART_STATUS_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_VUART_STATUS_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_VUART_STATUS_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_VUART_STATUS_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_TB_RESULT_REG_reg_t(Structure):
    _fields_ = [
        ('result', c_uint32, 32),
    ]

KM_CSR_TB_RESULT_REG_REG_DEFAULT = 0x00000000

class KM_CSR_TB_RESULT_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_TB_RESULT_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_TB_RESULT_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_TB_RESULT_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_TB_SIGNATURE_REG_reg_t(Structure):
    _fields_ = [
        ('signature', c_uint32, 32),
    ]

KM_CSR_TB_SIGNATURE_REG_REG_DEFAULT = 0x00000000

class KM_CSR_TB_SIGNATURE_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_TB_SIGNATURE_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_TB_SIGNATURE_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_TB_SIGNATURE_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_TB_ERRCODE_REG_reg_t(Structure):
    _fields_ = [
        ('errcode', c_uint32, 32),
    ]

KM_CSR_TB_ERRCODE_REG_REG_DEFAULT = 0x00000000

class KM_CSR_TB_ERRCODE_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_TB_ERRCODE_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_TB_ERRCODE_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_TB_ERRCODE_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_TB_SUBTEST_REG_reg_t(Structure):
    _fields_ = [
        ('subtest', c_uint32, 32),
    ]

KM_CSR_TB_SUBTEST_REG_REG_DEFAULT = 0x00000000

class KM_CSR_TB_SUBTEST_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_TB_SUBTEST_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_TB_SUBTEST_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_TB_SUBTEST_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_TB_CMD_REG_reg_t(Structure):
    _fields_ = [
        ('cmd', c_uint32, 32),
    ]

KM_CSR_TB_CMD_REG_REG_DEFAULT = 0x00000000

class KM_CSR_TB_CMD_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_TB_CMD_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_TB_CMD_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_TB_CMD_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_TB_CMD_ARG_REG_reg_t(Structure):
    _fields_ = [
        ('arg', c_uint32, 32),
    ]

KM_CSR_TB_CMD_ARG_REG_REG_DEFAULT = 0x00000000

class KM_CSR_TB_CMD_ARG_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_TB_CMD_ARG_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_TB_CMD_ARG_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_TB_CMD_ARG_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_TB_CMD_STATUS_REG_reg_t(Structure):
    _fields_ = [
        ('status', c_uint32, 32),
    ]

KM_CSR_TB_CMD_STATUS_REG_REG_DEFAULT = 0x00000000

class KM_CSR_TB_CMD_STATUS_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_TB_CMD_STATUS_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_TB_CMD_STATUS_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_TB_CMD_STATUS_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_TB_CMD_RESULT_REG_reg_t(Structure):
    _fields_ = [
        ('result', c_uint32, 32),
    ]

KM_CSR_TB_CMD_RESULT_REG_REG_DEFAULT = 0x00000000

class KM_CSR_TB_CMD_RESULT_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_TB_CMD_RESULT_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_TB_CMD_RESULT_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_TB_CMD_RESULT_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance

class KM_CSR_DEBUG_REG_reg_t(Structure):
    _fields_ = [
        ('magic', c_uint32, 32),
    ]

KM_CSR_DEBUG_REG_REG_DEFAULT = 0xCAFEBEEF

class KM_CSR_DEBUG_REG_reg_u(Union):
    _fields_ = [
        ('val', c_uint32),
        ('f', KM_CSR_DEBUG_REG_reg_t),
    ]

    def __init__(self, *args, **kwargs):
        super(KM_CSR_DEBUG_REG_reg_u, self).__init__(*args, **kwargs)
        self.val = KM_CSR_DEBUG_REG_REG_DEFAULT

    def as_bytes(self):
        # Determine the size of 'val' based on its type
        size = 4 if isinstance(self.val, c_uint32) else 8
        return self.val.to_bytes(size, 'little')

    @classmethod
    def from_bytes(cls, byte_seq):
        instance = cls()
        instance.val = int.from_bytes(byte_seq, 'little')
        return instance
