NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98
SPECIAL_FLAGS ?=

INCLUDES = -Iinclude

SERVER_SOURCE ?= source/Server.cpp

SRCS = source/main.cpp \
	$(SERVER_SOURCE) \
       source/Listener.cpp \
	   source/ClientConnection.cpp\
	   source/HttpParser.cpp\
	   source/HttpRequest.cpp\
	   source/HttpResponse.cpp\
	   source/Configs.cpp \
       source/ServerConfig.cpp \
       source/LocationConfig.cpp \
	   source/utils.cpp

OBJS = $(patsubst source/%.cpp,objects/%.o,$(SRCS))

all: $(NAME)

integration-test-build:
	bash tests/build/build_test.sh "$(SPECIAL_FLAGS)"

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

objects/%.o: source/%.cpp
	mkdir -p objects
	$(CXX) $(CXXFLAGS) $(SPECIAL_FLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS)
	rm -rf objects

fclean: clean
	rm -f $(NAME)

re: fclean all

test:
	bash ./tests/run_all.sh

.PHONY: all clean fclean re test