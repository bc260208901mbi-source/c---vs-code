CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
LDFLAGS = -lsqlite3 -lpthread -lws2_32

SRCS = main.cpp database.cpp device_routes.cpp media_routes.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = signage_backend

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)