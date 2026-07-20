# CC ?= cc
# CFLAGS ?= -O2 -Wall

# all: sender receiver

# sender: sender.c
# 	$(CC) $(CFLAGS) -o sender sender.c

# receiver: receiver.c
# 	$(CC) $(CFLAGS) -o receiver receiver.c

# clean:
# 	rm -f sender receiver


# CC ?= cc
# CFLAGS ?= -O2 -Wall -pthread

# all: sender receiver

# sender: sender.c
# 	$(CC) $(CFLAGS) -o sender sender.c

# receiver: receiver.c
# 	$(CC) $(CFLAGS) -o receiver receiver.c

# clean:
# 	rm -f sender receiver




CXX ?= g++
CXXFLAGS ?= -O3 -Wall -std=c++17 -pthread

all: sender receiver

sender: sender.cpp
	$(CXX) $(CXXFLAGS) -o sender sender.cpp

receiver: receiver.cpp
	$(CXX) $(CXXFLAGS) -o receiver receiver.cpp

clean:
	rm -f sender receiver