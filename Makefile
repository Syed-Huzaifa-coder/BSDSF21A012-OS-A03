CC = gcc
CFLAGS = -Iinclude -Wall
LDFLAGS = -lreadline
OBJDIR = obj
BINDIR = bin
SRCDIR = src

OBJS = $(OBJDIR)/main.o $(OBJDIR)/shell.o $(OBJDIR)/execute.o
TARGET = $(BINDIR)/myshell

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJDIR) $(BINDIR)
