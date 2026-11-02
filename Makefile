NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

INCLUDES = -Iinclude

SRCS = source/main.cpp \
       source/Server.cpp \
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

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

objects/%.o: source/%.cpp
	mkdir -p objects
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re