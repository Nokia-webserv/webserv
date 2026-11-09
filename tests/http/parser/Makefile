NAME := parser_test

ROOT_DIR := ../../..
SOURCE_DIR := $(ROOT_DIR)/source

CXX := c++
CXXFLAGS := -std=c++98 -Wall -Wextra
INCLUDES := -I$(SOURCE_DIR) -I$(ROOT_DIR)/include

SOURCES := parser_test.cpp \
			$(SOURCE_DIR)/HttpParser.cpp \
			$(SOURCE_DIR)/HttpRequest.cpp \
			$(SOURCE_DIR)/utils.cpp

.PHONY: all test clean fclean re

all: $(NAME)

$(NAME): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(SOURCES) -o $(NAME)

test: $(NAME)
	./$(NAME) fixtures

clean:
	rm -f $(NAME)

fclean: clean

re: fclean all