NAME = http_parser_class_tester

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

# Paths relative to this directory: webserv/tests/http/parser/
PROJECT_ROOT = ../../..
INCLUDES = -I$(PROJECT_ROOT)/include

OBJS = parser_test.o HttpParser.o HttpRequest.o ClientConnection.o

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

parser_test.o: parser_test.cpp $(PROJECT_ROOT)/include/ClientConnection.hpp \
	$(PROJECT_ROOT)/include/HttpParser.hpp \
	$(PROJECT_ROOT)/include/HttpRequest.hpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $< -o $@

HttpParser.o: $(PROJECT_ROOT)/source/HttpParser.cpp \
	$(PROJECT_ROOT)/include/HttpParser.hpp \
	$(PROJECT_ROOT)/include/HttpRequest.hpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $< -o $@

HttpRequest.o: $(PROJECT_ROOT)/source/HttpRequest.cpp \
	$(PROJECT_ROOT)/include/HttpRequest.hpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $< -o $@

ClientConnection.o: $(PROJECT_ROOT)/source/ClientConnection.cpp \
	$(PROJECT_ROOT)/include/ClientConnection.hpp \
	$(PROJECT_ROOT)/include/HttpParser.hpp \
	$(PROJECT_ROOT)/include/HttpRequest.hpp
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $< -o $@

run: $(NAME)
	./$(NAME)

clean:
	$(RM) $(OBJS)

fclean: clean
	$(RM) $(NAME)

re: fclean
	$(MAKE) all

.PHONY: all run clean fclean re
