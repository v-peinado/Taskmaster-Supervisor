#Style
GREEN	=	\033[92;5;118m
YELLOW	=	\033[93;5;226m
GRAY	=	\033[33;2;37m
RESET	=	\e[0m
CURSIVE	=	\033[33;3m
RED		= 	\033[31m

#Program names
SERVER	= taskmasterd
CLIENT	= taskmasterctl

#Compiler
CC		= c++
CFLAGS	=  -std=c++20 -D_GNU_SOURCE#-Wall -Wextra -Werror
#CFLAGS	+= -g3 -fsanitize=address
RM		= rm -f

#Includes
INC		= -I ./include/common/ -I ./include/server/ -I ./include/client/

#Libraries
LIBS	= -lyaml-cpp
CLIENT_LIBS	= -lreadline

#Source files
SRC_DIR	= src/

COMMON_SRC	= $(shell find $(SRC_DIR)common -type f -iname "*.cpp" | sed 's|^src/||')
SERVER_SRC	= $(shell find $(SRC_DIR)server -type f -iname "*.cpp" | sed 's|^src/||') main_server.cpp
CLIENT_SRC	= $(shell find $(SRC_DIR)client -type f -iname "*.cpp" 2>/dev/null | sed 's|^src/||') main_client.cpp

#Object files
OBJ_DIR	= obj/

COMMON_OBJ	= $(addprefix $(OBJ_DIR), $(COMMON_SRC:.cpp=.o))
SERVER_OBJ	= $(addprefix $(OBJ_DIR), $(SERVER_SRC:.cpp=.o))
CLIENT_OBJ	= $(addprefix $(OBJ_DIR), $(CLIENT_SRC:.cpp=.o))

all: obj $(SERVER) $(CLIENT)

obj:
	@mkdir -p $(OBJ_DIR)common $(OBJ_DIR)server $(OBJ_DIR)client

$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp | obj
	@$(CC) $(CFLAGS) $(INC) -c $< -o $@

$(SERVER): $(COMMON_OBJ) $(SERVER_OBJ)
	@$(CC) $(CFLAGS) $(INC) -o $(SERVER) $(COMMON_OBJ) $(SERVER_OBJ) $(LIBS)
	@printf "$(GREEN)$(SERVER): OK!$(RESET)\n"

$(CLIENT): $(COMMON_OBJ) $(CLIENT_OBJ)
	@$(CC) $(CFLAGS) $(INC) -o $(CLIENT) $(COMMON_OBJ) $(CLIENT_OBJ) $(CLIENT_LIBS)
	@printf "$(GREEN)$(CLIENT): OK!$(RESET)\n"

clean:
	@$(RM) -Rf $(OBJ_DIR)
	@printf "$(YELLOW)Object files: $(RED)Deleted!$(RESET)\n"

fclean: clean
	@$(RM) -f $(SERVER) $(CLIENT)
	@printf "$(YELLOW)$(SERVER) $(CLIENT): $(RED)Deleted!$(RESET)\n"

re: fclean all

.PHONY: all re clean fclean obj