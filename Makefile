# name of your application -- needs to match Cargo.toml crate name
APPLICATION = thread_duell

# If no BOARD is found in the environment, use this default:
BOARD ?= native

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../RIOT

USEMODULE += xtimer
USEMODULE += sema
USEMODULE += sched_cb

WPEDANTIC := 0
WERROR := 0


# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include $(RIOTBASE)/Makefile.include
