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
	   source/utils.cpp \
	   source/Router.cpp

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

build:
	bash tests/build/build_test.sh

startup:
	bash tests/startup/run_tests.sh

config:
	bash tests/config/test_config.sh

accept:
	bash tests/tcp/accept_test/accept_test.sh

parser:
	bash tests/http/parser/parser_test.sh

integration:
	bash tests/integration/run_tests.sh

timeout-test: $(NAME)
	bash tests/timeout/run_tests.sh ./webserv tests/timeout/timeout_test.conf

.PHONY: all clean fclean re build startup config accept parser integration timeout-test