NAME = http_parser_body

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

INCLUDES = -I../../../include

SRCS = body_test.cpp \
	../../../source/HttpParser.cpp \
	../../../source/HttpRequest.cpp \
	../../../source/ClientConnection.cpp \
	../../../source/utils.cpp

OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

run: $(NAME)
	./$(NAME)

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean
	$(MAKE) all

.PHONY: all run clean fclean re
