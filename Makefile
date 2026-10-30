NAME = webserv

CXX = c++
CXXFLAGS = -Wall -Wextra -Werror -std=c++98

INCLUDES = -Iinclude

SRCS = source/main.cpp \
       source/Server.cpp \
       source/tempServerConfig.cpp \
       source/Listener.cpp \
	   source/ClientConnection.cpp\
	   source/HttpParser.cpp\
	   source/HttpRequest.cpp\
	   source/HttpResponse.cpp\
	   source/utils.cpp


OBJS = $(SRCS:.cpp=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re