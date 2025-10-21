CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g

TARGET = server
OBJS = ../core/log/*.cpp ../core/pool/*.cpp ../core/timer/*.cpp \
       ../core/http/*.cpp ../core/server/*.cpp \
       ../core/buffer/*.cpp ../core/main.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o ../bin/$(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)
