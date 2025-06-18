

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <iostream>
#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
#define SERVER_IP "192.168.200.88"

void inicializarWinsock(int &bytesRecibidos, WSADATA &wsaData)
{
    // Inicializar winsock
    bytesRecibidos = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (bytesRecibidos != 0)
    {
        printf("WSAStartup fallo con el error: %d\n", bytesRecibidos);
        exit(1);
    }
}

void inicializarVariablesPistas(addrinfo &pistas)
{
    ZeroMemory(&pistas, sizeof(pistas));
    pistas.ai_family = AF_INET;
    pistas.ai_socktype = SOCK_STREAM;
    pistas.ai_protocol = IPPROTO_TCP;
    pistas.ai_flags = AI_PASSIVE;
}

void obtenerInformacionServidor(int &bytesRecibidos, addrinfo &pistas, addrinfo *&resultadoDireccion)
{
    // Resolver el nombre del servidor y el puerto
    bytesRecibidos = getaddrinfo(SERVER_IP, DEFAULT_PORT, &pistas, &resultadoDireccion);
    if (bytesRecibidos != 0)
    {
        printf("getaddrinfo fallo con el error: %d\n", bytesRecibidos);
        WSACleanup();
        exit(1);
    }
}

void crearSocketEscucha(SOCKET &socketEscucha, addrinfo *resultadoDireccion)
{
    // Crear un socket del servidor que escuche conexiones de clientes
    socketEscucha = socket(resultadoDireccion->ai_family, resultadoDireccion->ai_socktype, resultadoDireccion->ai_protocol);
    if (socketEscucha == INVALID_SOCKET)
    {
        printf("socket fallo con el error: %ld\n", WSAGetLastError());
        freeaddrinfo(resultadoDireccion);
        WSACleanup();
        exit(1);
    }
}

void configurarSocketEscucha(int &bytesRecibidos, SOCKET &socketEscucha, addrinfo *resultadoDireccion)
{
    // Configurar el socket de escucha
    bytesRecibidos = bind(socketEscucha, resultadoDireccion->ai_addr, (int)resultadoDireccion->ai_addrlen);
    if (bytesRecibidos == SOCKET_ERROR)
    {
        printf("bind fallo con el error: %d\n", WSAGetLastError());
        freeaddrinfo(resultadoDireccion);
        closesocket(socketEscucha);
        WSACleanup();
        exit(1);
    }
}

void escuchar(int &bytesRecibidos, SOCKET &socketEscucha)
{
    bytesRecibidos = listen(socketEscucha, SOMAXCONN);
    if (bytesRecibidos == SOCKET_ERROR)
    {
        printf("Escuchar fallo con el error: %d\n", WSAGetLastError());
        closesocket(socketEscucha);
        WSACleanup();
        exit(1);
    }
}

void aceptarSocketCliente(SOCKET &socketCliente, SOCKET &socketEscucha)
{
    // Aceptar un socket de cliente
    socketCliente = accept(socketEscucha, NULL, NULL);
    if (socketCliente == INVALID_SOCKET)
    {
        printf("Aceptado cliente pero fallo con el error %d\n", WSAGetLastError());
        closesocket(socketEscucha);
        WSACleanup();
        exit(1);
    }
}

int main(void)
{
    WSADATA wsaData;
    int bytesRecibidos;

    SOCKET socketEscucha = INVALID_SOCKET;
    SOCKET socketCliente = INVALID_SOCKET;

    struct addrinfo *resultadoDireccion = NULL;
    struct addrinfo pistas;

    int bytesEnviados;
    char bufferRecibido[DEFAULT_BUFLEN];
    int bufferRecibidoLen = DEFAULT_BUFLEN;

    inicializarWinsock(bytesRecibidos, wsaData);

    inicializarVariablesPistas(pistas);

    obtenerInformacionServidor(bytesEnviados, pistas, resultadoDireccion);

    crearSocketEscucha(socketEscucha, resultadoDireccion);

    configurarSocketEscucha(bytesRecibidos, socketEscucha, resultadoDireccion);

    freeaddrinfo(resultadoDireccion);

    escuchar(bytesRecibidos, socketEscucha);

    printf("Servidor inicializado\n");

    while (true)
    {

        aceptarSocketCliente(socketCliente, socketEscucha);
        bytesRecibidos = recv(socketCliente, bufferRecibido, bufferRecibidoLen, 0);

        if (bytesRecibidos > 0)
        {

            bufferRecibido[bytesRecibidos] = '\0';
            printf("Bytes recibidos: %d\n", bytesRecibidos);
            printf("Mensaje recibido: %s\n", bufferRecibido);

            const char *mensaje = "Ok";
            bytesEnviados = send(socketCliente, mensaje, (int)strlen(mensaje), 0);

            if (bytesEnviados == SOCKET_ERROR)
            {
                printf("Fallo el envio con el error: %d\n", WSAGetLastError());
                closesocket(socketCliente);
                WSACleanup();
                return 1;
            }
            printf("Bytes enviados del servidor: %d\n", bytesEnviados);
        }
    }

    // No longer need server socket
    closesocket(socketEscucha);

    // shutdown the connection since we're done
    bytesRecibidos = shutdown(socketCliente, SD_SEND);
    if (bytesRecibidos == SOCKET_ERROR)
    {
        printf("Cerrando con el error: %d\n", WSAGetLastError());
        closesocket(socketCliente);
        WSACleanup();
        return 1;
    }

    // cleanup
    closesocket(socketCliente);
    WSACleanup();

    return 0;
}