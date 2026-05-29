CXX      = g++
CXXFLAGS = -g -Wall -Wextra -pthread -std=c++11
TARGET   = dineseating

# Object files.  Note: we compile .c files using -x c++ so they are
# treated as C++ sources by g++ even though they have a .c suffix.
OBJS     = main.o request_queue.o producers.o consumers.o log.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CXX) $(CXXFLAGS) -x c++ -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean
