# temp Makefile for testing

.PHONY: all clean fclean re run

all:
	$(MAKE) -f config/test/Makefile

clean:
	$(MAKE) -f config/test/Makefile clean

fclean:
	$(MAKE) -f config/test/Makefile fclean

re:
	$(MAKE) -f config/test/Makefile re

run:
	$(MAKE) -f config/test/Makefile run