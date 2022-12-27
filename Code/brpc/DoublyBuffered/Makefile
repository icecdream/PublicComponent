CC=gcc
CXX=g++
INC_PATH= .

O_FLAG = -O0
CFLAGS    += ${O_FLAG} -Wno-deprecated -Wall -std=c++11
#CFLAGS    += -fno-elide-constructors
LDFLAGS   =  -L/usr/lib -L/usr/local/lib -lpthread 
CFLAGS += -I$(INC_PATH) $(INCLUDE) -g


# 输出文件名
TARGET= ./bin/Test
OUTPUT_PATH = ./obj


#设置VPATH 包含源码的子目录列表
#添加源文件
SUBINC = dbd .

#添加头文件
SUBDIR = dbd .

#设置VPATH
INCLUDE = $(foreach n, $(SUBINC), -I$(INC_PATH)/$(n)) 
SPACE =  
VPATH = $(subst $(SPACE),, $(strip $(foreach n,$(SUBDIR), $(INC_PATH)/$(n)))) $(OUTPUT_PATH)

C_SOURCES = $(notdir $(foreach n, $(SUBDIR), $(wildcard $(INC_PATH)/$(n)/*.c)))
CPP_SOURCES = $(notdir $(foreach n, $(SUBDIR), $(wildcard $(INC_PATH)/$(n)/*.cpp)))
CC_SOURCES = $(notdir $(foreach n, $(SUBDIR), $(wildcard $(INC_PATH)/$(n)/*.cc)))

C_OBJECTS = $(patsubst  %.c,  $(OUTPUT_PATH)/%.o, $(C_SOURCES))
CPP_OBJECTS = $(patsubst  %.cpp,  $(OUTPUT_PATH)/%.o, $(CPP_SOURCES))
CC_OBJECTS = $(patsubst  %.cc,  $(OUTPUT_PATH)/%.o, $(CC_SOURCES))

CXX_SOURCES = $(CPP_SOURCES) $(C_SOURCES) $(CC_SOURCES)
CXX_OBJECTS = $(CPP_OBJECTS) $(C_OBJECTS) $(CC_OBJECTS)


$(TARGET):$(CXX_OBJECTS)
	$(CXX) -o $@ $(foreach n, $(CXX_OBJECTS), $(n)) $(foreach n, $(OBJS), $(n))  $(LDFLAGS) 
	#******************************************************************************#
	#                               Build successful !                             #
	#******************************************************************************#
	
$(OUTPUT_PATH)/%.o:%.cpp
	$(CXX) $< -c $(CFLAGS) -o $@
	
$(OUTPUT_PATH)/%.o:%.c
	$(CC) $< -c $(CFLAGS) -o $@

$(OUTPUT_PATH)/%.o:%.cc
	$(CXX) $< -c $(CFLAGS) -o $@

mkdir:
	mkdir -p $(dir $(TARGET))
	mkdir -p $(OUTPUT_PATH)
	
rmdir:
	rm -rf $(dir $(TARGET))
	rm -rf $(OUTPUT_PATH)

clean:
	rm -f $(OUTPUT_PATH)/*
	rm -rf $(TARGET)

