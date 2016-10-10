#
# Single makefile to build all components of cloudtiering.
#


### versions
MAJOR_VER ::= 0
MINOR_VER ::= 1
PATCH_VER ::= 0   
VERSION   ::= ${MAJOR_VER}.${MINOR_VER}.${PATCH_VER}


### names
APP_NAME   ::= fs-monitor
LIB_SONAME ::= libcloudtiering.so
LIB_NAME   ::= ${LIB_SONAME}.${VERSION}
TST_NAME   ::= test-suit


### directories
INC_DIR    ::= inc
BIN_DIR    ::= bin
SRC_DIR    ::= src
APP_SUBDIR ::= app
LIB_SUBDIR ::= lib
TST_SUBDIR ::= tst


### lists of source files
LIB_SRC ::= $(wildcard ${SRC_DIR}/${LIB_SUBDIR}/*.c)
APP_SRC ::= $(wildcard ${SRC_DIR}/${APP_SUBDIR}/*.c)
TST_SRC ::= $(wildcard ${SRC_DIR}/${TST_SUBDIR}/*.c)


### lists of produced objects
LIB_OBJ ::= $(patsubst ${SRC_DIR}/${LIB_SUBDIR}/%.c,${BIN_DIR}/${LIB_SUBDIR}/%.o,${LIB_SRC})
APP_OBJ ::= $(patsubst ${SRC_DIR}/${APP_SUBDIR}/%.c,${BIN_DIR}/${APP_SUBDIR}/%.o,${APP_SRC})
TST_OBJ ::= $(patsubst ${SRC_DIR}/${TST_SUBDIR}/%.c,${BIN_DIR}/${TST_SUBDIR}/%.o,${TST_SRC})


### list of linked external libraries
LNK_LIB ::= dotconf


### targets
all: lib app tst


lib: mkdir-${BIN_DIR}/${LIB_SUBDIR} ${LIB_OBJ}
	@echo "BUILDING lib"


app: mkdir-${BIN_DIR}/${APP_SUBDIR} lib ${APP_OBJ}
	gcc -l${BIN_DIR}/${LIB_NAME} -o ${APP_NAME}


tst: mkdir-${BIN_DIR}/${TST_SUBDIR} lib ${TST_OBJ}
	gcc $(addprefix -l,${LNK_LIB} ) -o${TST_NAME} ${TST_OBJ}


${BIN_DIR}/%.o: ${SRC_DIR}/*/%.c
	gcc -g -std=c11 -Wall $(addprefix -I,${INC_DIR}) $(addprefix -l,${LNK_LIB}) -c$< -o$@


mkdir-${BIN_DIR}/%:
	mkdir --parents $(subst mkdir-,,$@)


clean:
	rm --recursive --force ${BIN_DIR}


clean-%: clean-${BIN_DIR}/%
	rm --force $($(shell tr '[:lower:]' '[:upper:]' <<< $(subst clean-${BIN_DIR}/,,$<))_NAME)


clean-${BIN_DIR}/%:
	rm --recursive --force $(subst clean-,,$@)

