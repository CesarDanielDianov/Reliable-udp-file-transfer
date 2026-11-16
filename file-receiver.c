#include "packet-format.h"
#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <sys/time.h>

int file_completo=0;

int main(int argc, char *argv[]) {

  if(argc!=4){                // VERIFICAÇÃO DO NUMERO DE ARGUMENTOS
    exit(EXIT_FAILURE);
  }

  char *file_name = argv[1];
  int port = atoi(argv[2]);
  int window_size = atoi(argv[3]);

  if (window_size>MAX_WINDOW_SIZE){               // O TAMANHO DA WINDOWS NAO PODE EXCEDER 32
    exit(EXIT_FAILURE);
  }


  FILE *file = fopen(file_name, "w");
  if (!file) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }

  // Prepare server socket.
  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  // Allow address reuse so we can rebind to the same port,
  // after restarting the server.
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) <
      0) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in srv_addr = {
      .sin_family = AF_INET,
      .sin_addr.s_addr = htonl(INADDR_ANY),
      .sin_port = htons(port),
  };
  if (bind(sockfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr))) {
    perror("bind");
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "Receiving on port: %d\n", port);

  ssize_t len;


  int base=0;

  struct timeval tv;
  tv.tv_sec=4;
  tv.tv_usec=0;    //  <-- isto tava a dar pronlema pois nao era iniciado (casas deciamais dos segundos)

  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));    //define um comportamento pros sockets  SO_RCVTIMEO=>receive timeout
 

  int last_chunk=-1;

  uint32_t mask = 0;



  do { // Iterate over segments, until last the segment is detected.
    // Receive segment.

    struct sockaddr_in src_addr;

    data_pkt_t data_pkt;
    ack_pkt_t ack_pkt;  



    len = recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0,(struct sockaddr *)&src_addr, &(socklen_t){sizeof(src_addr)}); //RECEBEMOS UM CHUNK

    if (len < sizeof(data_pkt)){  //SE O CHUNK RECEBIDO É MENOR QUE 1000 ENT É PK ESTAMOS NO ULTIMO CHUNK
      last_chunk=ntohl(data_pkt.seq_num);
    }

    if (len==-1 ){//TIMEOUT DO RECEIVER
      
      printf("O RECEIVER DEU TIMEOUT DE 4S\n");
      if (file_completo==1){
        printf("O FILE REALMNET FICA COMPLETO\n");
        exit(EXIT_SUCCESS);
      }
      fclose(file);
      remove(file_name);
      printf("O FILE NAO FOI COMPLETADO\n");
      exit(EXIT_FAILURE);
    }

    int seq_num = ntohl(data_pkt.seq_num);

  
    if (seq_num < base+window_size  &&  seq_num > base){   //VEMOS SE O CHUNK ESTA DENTRO DA WINDOW E NAO É BASE,SE SIM ,WRITE 
      fseek(file, seq_num*1000 ,SEEK_SET);                          //1-file pro escreber ;2-deslocamennto por escrita(1000B);3-origem da escrita
      fwrite(data_pkt.data, 1, len - offsetof(data_pkt_t, data), file);

      ack_pkt.seq_num=htonl(base);  
      int N = seq_num-base-1;    //SIMPLESMNETE METEMSO O VALOR QUE NAO É A BASE ,NA MASK ,COMO RECEBIDO (1)
      mask= mask | (1 << N);
      ack_pkt.selective_acks=htonl(mask);  
      sendto(sockfd, &ack_pkt,sizeof(ack_pkt), 0,(struct sockaddr *)&src_addr, sizeof(src_addr));
    }


    else if (seq_num == base){    //VEMOS SE O CHUNK É A BASE ,SE SIM ,WRITE 
      fseek(file, base*1000 ,SEEK_SET);  
      fwrite(data_pkt.data, 1, len - offsetof(data_pkt_t, data), file);
      //ATUALIZAÇAO DA BASE DA WINDOW   
      if((mask & 1) == 1){
        while((mask & 1) == 1){  
          mask = mask >> 1; //shiftar pra direita em 1 
          base++;
        }
      }
      if((mask & 1)==0){
        mask = mask >> 1;
        base++;
      }
      ack_pkt.seq_num=htonl(base); 
      ack_pkt.selective_acks=htonl(mask);  
      sendto(sockfd, &ack_pkt,sizeof(ack_pkt), 0,(struct sockaddr *)&src_addr, sizeof(src_addr));
    }   

    //SE FOREM CHUNKS FORA DA JANLEA ,DEVEMOS APENAS DAR UM ACK SEM ESCREVER NADA
    else if(seq_num >= base+window_size  ||  seq_num < base){
      ack_pkt.seq_num=htonl(base); 
      ack_pkt.selective_acks=htonl(mask);  
      sendto(sockfd, &ack_pkt,sizeof(ack_pkt), 0,(struct sockaddr *)&src_addr, sizeof(src_addr));
    }


    printf("Received segment %d, size %ld.\n", ntohl(data_pkt.seq_num), len); 




    if(last_chunk != -1 && last_chunk < base){
      file_completo=1;
    }
  } while (true);



  // Clean up and exit.
  close(sockfd);
  fclose(file);

  return EXIT_SUCCESS;
}
