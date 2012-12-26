#
# image should be loaded at $(TEXT_BASE)
#

ifeq ($(LARGEKERNEL),1)
TEXT_BASE = 0x01100000
else
TEXT_BASE = 0x00600000
endif
