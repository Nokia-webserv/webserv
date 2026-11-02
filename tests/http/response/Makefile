NAME = http_response_tester

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

# Paths relative to this directory: webserv/tests/http/response/
PROJECT_ROOT = ../../..
RESPONSE_CPP = $(PROJECT_ROOT)/source/HttpResponse.cpp
RESPONSE_HPP = $(PROJECT_ROOT)/include/HttpResponse.hpp
INCLUDES = -I$(PROJECT_ROOT)/include

OBJS = http_response_tester.o HttpResponse.o

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

http_response_tester.o: http_response_tester.cpp $(RESPONSE_HPP)
	$(CXX) $(INCLUDES) $(CXXFLAGS) -c $< -o $@

HttpResponse.o: $(RESPONSE_CPP) $(RESPONSE_HPP)
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
