//
// Created by Juan Salazar on 22/11/23.
//

#include "TftpServer.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <chrono>

TftpServer::TftpServer()
= default;

/**
 * @brief Starts the TFTP server.
 * 
 * This function initializes the TFTP server by creating a socket, binding it to a specific port,
 * and listening for incoming TFTP requests. It handles both read (RRQ) and write (WRQ) requests.
 * For read requests, it opens the requested file, reads its content, and sends it to the client
 * in TFTP data packets. For write requests, it receives data packets from the client and writes
 * them to the requested file.
 * 
 * @note This function runs indefinitely until the server is stopped.
 */
void TftpServer::start()
{
    std::cout << "Starting TFTP server..." << std::endl;

    struct timeval stTimeOut{};

    stTimeOut.tv_sec = 5;
    stTimeOut.tv_usec = 0;

    int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket < 0)
    {
        std::cout << "Error creating socket" << std::endl;
        return;
    }

    setsockopt(serverSocket, SOL_SOCKET, SO_RCVTIMEO, &stTimeOut, sizeof(stTimeOut));

    struct sockaddr_in serverAddress;

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(69);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        std::cout << "Error binding socket" << std::endl;
        close(serverSocket); // Cierra el socket antes de salir
        return;
    }

    std::cout << "Server address: " << inet_ntoa(serverAddress.sin_addr) << std::endl;
    std::cout << "Server port: " << ntohs(serverAddress.sin_port) << std::endl;

    while (true)
    {
        char buffer[1024]; // Tamaño suficiente para los paquetes TFTP
        struct sockaddr_in clientAddress;
        socklen_t clientLength = sizeof(clientAddress);

        int n = recvfrom(serverSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddress, &clientLength);
        if (n < 0)
        {
            std::cout << "Listening..." << std::endl;
                // Podrías enviar un mensaje de error al cliente si se espera una respuesta específica
                continue; // Continúa con la siguiente iteración del bucle
            }

            // Parsear el opcode del paquete TFTP
            unsigned short opcode = (buffer[0] << 8) | buffer[1];

            // print opcode

            std::cout << "Opcode: " << opcode << std::endl;

            // Client address

            std::cout << "Client address: " << inet_ntoa(clientAddress.sin_addr) << std::endl;

            // Client length

            std::cout << "Client length: " << clientLength << std::endl;

            if (opcode == 1)
            { // RRQ - Solicitud de lectura
                // Se asume que el paquete TFTP contiene el nombre del archivo solicitado
                std::string filename(buffer + 2); // Se obtiene el nombre del archivo desde el paquete

                int ackblockerrors = 0;

                std::cout << "Filename: " << filename << std::endl;

                // Lógica para manejar la solicitud de lectura y enviar el archivo solicitado
                // - Abrir el archivo solicitado
                // - Leer su contenido
                // - Enviar los datos en paquetes TFTP al cliente

                FILE *file = fopen(filename.c_str(), "rb"); // Abre el archivo en modo lectura binaria
                if (!file)
                {
                    std::cout << "Error al abrir el archivo solicitado" << std::endl;
                    // Send error message to the client
                    std::string errorMessage = "Error: Failed to open the requested file";
                    sendto(serverSocket, errorMessage.c_str(), errorMessage.length(), 0, (struct sockaddr *)&clientAddress, clientLength);
                    break;
                }
                else
                {
                    // Get the file size
                    fseek(file, 0, SEEK_END);
                    long fileSize = ftell(file);
                    rewind(file);

                    // Read and send the file in data blocks
                    const int blockSize = 516; // Maximum size of a data block in TFTP
                    char dataBuffer[blockSize];

                    int blockNumber = 1;

                    while (fileSize > 0)
                    {
                        // TFTP DATA packet header

                        // Block number low byte

                        // Create TFTP DATA packet
                        // Read file data from dataBuffer[4]
                        dataBuffer[0] = 0; // Opcode DATA
                        dataBuffer[1] = 3;
                        dataBuffer[2] = (blockNumber >> 8) & 0xFF; // Block number high byte
                        dataBuffer[3] = blockNumber & 0xFF;
                        int bytesRead = fread(dataBuffer + 4, 1, blockSize - 4, file); // Read file data
                        // print dataBuffer size
                        std::cout << "Data buffer size: " << sizeof(dataBuffer) << std::endl;
                        if (bytesRead < 0)
                        {
                            std::cout << "Error reading the file" << std::endl;
                            // Send error message to the client
                            std::string errorMessage = "Error: Failed to read the file";
                            sendto(serverSocket, errorMessage.c_str(), errorMessage.length(), 0, (struct sockaddr *)&clientAddress, clientLength);
                            break;
                        }

                        std::cout << "Sending block number: " << blockNumber << std::endl;

                        ssize_t n = sendto(serverSocket, dataBuffer, bytesRead + 4, 0, (struct sockaddr *)&clientAddress, clientLength);
                        // Confirm that all data has been sent

                        std::cout << "Bytes sent: " << n << std::endl;

                        if (n < 0)
                        {
                            std::cout << "Error sending data to the client" << std::endl;
                            // Send error message to the client
                            std::string errorMessage = "Error: Failed to send data to the client";
                            sendto(serverSocket, errorMessage.c_str(), errorMessage.length(), 0, (struct sockaddr *)&clientAddress, clientLength);
                            break;
                        }

                        fileSize -= bytesRead;
                        blockNumber++;

                        // Wait for ACK from the client

                        bool ackReceived = false;

                        const int TIMEOUT_MS = 5000;

                        auto start_time = std::chrono::steady_clock::now();

                        while (!ackReceived && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start_time).count() < TIMEOUT_MS)
                        {

                            ssize_t n = recvfrom(serverSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddress, &clientLength);
                            if (n < 0)
                            {
                                std::cout << "Error receiving data" << std::endl;
                                // You could send an error message to the client if a specific response is expected
                                continue; // Continue with the next iteration of the loop
                            }

                            // Parse the opcode of the TFTP packet
                            unsigned short opcode = (buffer[0] << 8) | buffer[1];

                            if (opcode == 4)
                            {
                                // get block number
                                unsigned short ACKblockNumber = ((static_cast<unsigned short>(buffer[2]) << 8) & 0xFF00) | (static_cast<unsigned short>(buffer[3]) & 0x00FF);
                                std::cout << "ACK Block number: " << ACKblockNumber << std::endl;
                                if (ACKblockNumber == blockNumber - 1)
                                {
                                    ackReceived = true;
                                }
                                else
                                {
                                    std::cout << "ACK Block number incorrect" << std::endl;
                                    std::cout << "Resending block number: " << blockNumber - 1 << std::endl;
                                    ssize_t n = sendto(serverSocket, dataBuffer, bytesRead + 4, 0, (struct sockaddr *)&clientAddress, clientLength);
                                    ackblockerrors++;
                                }
                            }
                            else
                            {
                                std::cout << "ACK Opcode incorrect" << std::endl;
                            }
                        }
                    }

                    std::cout << "ACK block errors: " << ackblockerrors << std::endl;

                    fclose(file); // Close the file after the transfer
                }
            }
            else if (opcode == 2)
            {
                // WRQ - Write request
                // It is assumed that the TFTP packet contains the requested file name
                std::string filename(buffer + 2); // Get the file name from the packet

                std::cout << "Filename: " << filename << std::endl;

                // Logic to handle the write request and receive the requested file
                // - Open the requested file
                // - Receive the data in TFTP packets from the client
                // - Write the data to the file

                FILE *file = fopen(filename.c_str(), "wb"); // Open the file in binary write mode
                if (!file)
                {
                    std::cout << "Error opening the requested file" << std::endl;
                    // Send error message to the client
                    std::string errorMessage = "Error: Failed to open the requested file";
                    sendto(serverSocket, errorMessage.c_str(), errorMessage.length(), 0, (struct sockaddr *)&clientAddress, clientLength);
                    break;
                }
                else
                {
                    // Read and write the file in data blocks
                    const int blockSize = 516; // Maximum size of a data block in TFTP
                    char dataBuffer[blockSize];
                    int blockNumber = 0;
                    bool lastBlock = false;

                    // Initial ACK

                    char ackPacket[4];
                    ackPacket[0] = 0; // Opcode ACK
                    ackPacket[1] = 4;
                    ackPacket[2] = 0; // Block number high byte
                    ackPacket[3] = 0; // Block number low byte

                    n= sendto(serverSocket, ackPacket, sizeof(ackPacket), 0, (struct sockaddr *) &clientAddress,
                              clientLength);

                    // Transfer mode

                    std::cout << "Transfer mode: " << buffer + 2 + filename.length() + 1 << std::endl;

                    while (!lastBlock)
                    {
                        // Wait for DATA from the client

                        bool dataReceived = false;

                        while (!dataReceived)
                        {

                                        n = recvfrom(serverSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&clientAddress, &clientLength);
                                        if (n < 0)
                                        {
                                            std::cout << "Error receiving data" << std::endl;
                                            continue; // Continue with the next iteration of the loop
                                        }

                                        // Parse the opcode of the TFTP packet
                                        unsigned short opcode = (buffer[0] << 8) | buffer[1];

                                        if (opcode == 3)
                                        {
                                            // get block number
                                            unsigned short DATAblockNumber = ((static_cast<unsigned short>(buffer[2]) << 8) & 0xFF00) | (static_cast<unsigned short>(buffer[3]) & 0x00FF);
                                            std::cout << "DATA Block number: " << DATAblockNumber << std::endl;
                                            if (DATAblockNumber == blockNumber + 1)
                                            {
                                                dataReceived = true;
                                                // ACK
                                                char ackPacket[4];
                                                ackPacket[0] = 0; // Opcode ACK
                                                ackPacket[1] = 4;
                                                ackPacket[2] = buffer[2]; // Block number high byte
                                                ackPacket[3] = buffer[3]; // Block number low byte

                                                fwrite(buffer + 4, 1, n - 4, file);

                                                if (n - 4 < blockSize - 4)
                                                {
                                                    lastBlock = true;
                                                }

                                                blockNumber++;

                                                n = sendto(serverSocket, ackPacket, sizeof(ackPacket), 0, (struct sockaddr *)&clientAddress, clientLength);

                                                if (n < 0)
                                                {
                                                    std::cout << "Error sending ACK" << std::endl;
                                                }
                                            }
                                            else
                                            {
                                                std::cout << "DATA Block number incorrect" << std::endl;
                                            }
                                        }
                                        else
                                        {
                                            std::cout << "DATA Opcode incorrect" << std::endl;
                                        }
                                    }
                                }

                                fclose(file); // Close the file after the transfer
                            }
                        }
                        else if (opcode == 5)
                        { 
                            // Error
                            // It is assumed that the TFTP packet contains the error code and error message
                            unsigned short errorCode = (buffer[2] << 8) | buffer[3];
                            std::string errorMessage(buffer + 4);
                            std::cout << "Error code: " << errorCode << std::endl;
                            std::cout << "Error message: " << errorMessage << std::endl;
                        }
                        else
                        {
                            std::cout << "Unknown opcode: " << opcode << std::endl;
                            continue; 
                        }
                    }

                    close(serverSocket); // Close the socket when exiting the loop (not executed in this example)
                }
