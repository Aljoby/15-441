

default: echo_server echo_client

echo_server:
	@gcc cp1.c -o echo_server -Wall -Werror

echo_client:
	@gcc echo_client.c -o echo_client -Wall -Werror

clean:
	@rm echo_server echo_client