CXX = g++
CFLAGS = -std=c++14 -O2 -Wall -g

TARGET = test
OBJS = ../core/log/*.cpp ../core/pool/*.cpp ../core/timer/*.cpp \
       ../core/http/*.cpp ../core/server/*.cpp \
       ../core/buffer/*.cpp ../test/test.cpp

all: $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) -o $(TARGET)  -pthread -lmysqlclient

clean:
	rm -rf ../bin/$(OBJS) $(TARGET)
