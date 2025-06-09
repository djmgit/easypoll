import socket

def server_con():
    server_address = ("127.0.0.1", 55555)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect(server_address)
    sock.send("hi".encode())
    res = ""

    while True:
        print ("Trying to fetch\n")
        data = sock.recv(1)
        print (data)
        if not data or data == "\0".encode():
            break
        res += data.decode()
    
    print (res)
    sock.close()


if __name__ == "__main__":
    server_con()
        