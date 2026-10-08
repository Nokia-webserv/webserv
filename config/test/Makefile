COMPILER = c++
FLAGS = -g -Wall -Wextra -Werror -std=c++98

NAME = a.out
OBJ_DIR = obj

CONFIG_SRCS := $(wildcard config/*.cpp)
SRCS := main.cpp $(CONFIG_SRCS)

OBJS := obj/main.o \
		$(patsubst config/%.cpp,obj/config/%.o,$(CONFIG_SRCS))

INCLUDES := $(wildcard config/*.hpp)

RM = rm -rf

.PHONY: all clean fclean re run

all: $(NAME)

$(NAME): $(OBJS)
	$(COMPILER) $(FLAGS) -o $@ $^

obj/main.o: main.cpp $(INCLUDES)
	@mkdir -p $(dir $@)
	$(COMPILER) $(FLAGS) -c $< -o $@

obj/config/%.o: config/%.cpp $(INCLUDES)
	@mkdir -p $(dir $@)
	$(COMPILER) $(FLAGS) -c $< -o $@

clean:
	$(RM) $(OBJ_DIR)

fclean: clean
	$(RM) $(NAME)

re: fclean all

run: all
	valgrind ./$(NAME)