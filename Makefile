CXX = g++
CFLAGS = -Wall -std=c++17 -pthread

IMGUI_SRC = imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/backends/imgui_impl_glfw.cpp imgui/backends/imgui_impl_opengl3.cpp

all: server client

server:
	$(CXX) $(CFLAGS) server_main.cpp Server.cpp -lsqlite3 -o server

client:
	$(CXX) $(CFLAGS) client_main.cpp ChatClient.cpp $(IMGUI_SRC) -lglfw -lGL -o client

clean:
	rm -f client server