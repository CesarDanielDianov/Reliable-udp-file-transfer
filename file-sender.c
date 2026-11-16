#include "packet-format.h"
#include <limits.h>
#include <netdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {

  if (argc!=5){
    exit(EXIT_FAILURE);
  }

  char *file_name = argv[1];
  char *host = argv[2];
  int port = atoi(argv[3]);



  

  FILE *file = fopen(file_name, "r");
  if (!file) {
    perror("fopen");
    exit(EXIT_FAILURE);
  }

  // Prepare server host address.
  struct hostent *he;
  if (!(he = gethostbyname(host))) {
    perror("gethostbyname");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in srv_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(port),
      .sin_addr = *((struct in_addr *)he->h_addr),
  };

  int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  uint32_t seq_num = 0;
  data_pkt_t data_pkt;
  size_t data_len;

  int base=0;

  struct timeval tv;
  tv.tv_sec=1;
  tv.tv_usec=0;    //  <-- isto tava a dar pronlema pois nao era iniciado (casas deciamais dos segundos)

  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));    //define um comportamento pros sockets  SO_RCVTIMEO=>receive timeout







  int window_size=atoi(argv[4]);
  int slide_window=3; //0-NAO FAZ NADA  ;1-ENVIA O NEXT ;2-REENVIA A BASE ;3-REENVIA A WINDOW TODA
  int dup_acks=0;
  int timeout=0;

  fseek(file,0, SEEK_END);
  long xxx=ftell(file);
  fseek(file,0, SEEK_SET);
  int total=(xxx/1000)+1;

  int last_ack=0;
  uint32_t last_mask=0;

  do { // Generate segments from file, until the the end of the file.
    // Prepare data segment.



    if (slide_window==1 ){         
      if(seq_num < total){   //MANDAMOS O NEXT 
          fseek(file,seq_num*1000, SEEK_SET);
          data_pkt.seq_num = htonl(seq_num++);  

          
          data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);
          ssize_t sent_len = sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,(struct sockaddr *)&srv_addr, sizeof(srv_addr));
          printf("Sending segment %d, size %ld.\n", ntohl(data_pkt.seq_num),offsetof(data_pkt_t, data) + data_len);

          if (sent_len != offsetof(data_pkt_t, data) + data_len) {
            fprintf(stderr, "Truncated packet.\n");
            exit(EXIT_FAILURE);

        }
      }
    }

    else if (slide_window==2){            //MANDAMOS A BASE
        data_pkt.seq_num = htonl(base);  
        fseek(file,1000*base, SEEK_SET);
        data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);
        ssize_t sent_len = sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,(struct sockaddr *)&srv_addr, sizeof(srv_addr));
        printf("Sending segment %d, size %ld.\n", ntohl(data_pkt.seq_num),offsetof(data_pkt_t, data) + data_len);

        if (sent_len != offsetof(data_pkt_t, data) + data_len) {
          fprintf(stderr, "Truncated packet.\n");
          exit(EXIT_FAILURE);
        }


    }

    else if(slide_window==3){   //MANDA A WINDOW TODA 
      seq_num=base;

      for(int i=base; i<base+window_size && i<total ;i++){  
        data_pkt.seq_num = htonl(seq_num++);

        if(i<last_ack || (i>last_ack && (last_mask & (1u << (i - last_ack - 1))))){
          continue;
        }



        fseek(file,1000*i, SEEK_SET);
        data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);
        ssize_t sent_len = sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,(struct sockaddr *)&srv_addr, sizeof(srv_addr));
        
        printf("Sending segment %d, size %ld.\n", ntohl(data_pkt.seq_num),offsetof(data_pkt_t, data) + data_len);
        if (sent_len != offsetof(data_pkt_t, data) + data_len) {
          fprintf(stderr, "Truncated packet.\n");
          exit(EXIT_FAILURE);
        }

      }
    }




    
    ack_pkt_t ack_pkt; 
    ssize_t len=recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0,(struct sockaddr *)&srv_addr, &(socklen_t){sizeof(srv_addr)});  //ESPERAMOS PELO ACK DA BASE

    if(len==-1){  //NAO RECEBEMOS NADA
      slide_window=3;
      timeout++;
      if(timeout==3){
        exit(EXIT_FAILURE);
      }
      printf("REENVIAMOS A WINDOW TODA(houve time out)\n");
    }

    else{   //RECEBEMOS ALGUMA COISA
      timeout=0;
      last_ack = ntohl(ack_pkt.seq_num);
      last_mask = ntohl(ack_pkt.selective_acks);
      if(htonl(ack_pkt.seq_num)>base){  //SE FOR O ACKS DA BASE ,TEMOS DE AVANÇAR A WINDOW E MANDAR O ULTIMO ELEMENTO 
        dup_acks=0;
        base=last_ack;
        printf("ENVIAMOS NEXT(recebemos o ack base)\n");
        slide_window=1;
      }
      else if(htonl(ack_pkt.seq_num)!=base+1){
        printf("NAO FAZEMOS NADA(recebemos um dup e ainda n sao 3)\n");
        dup_acks++;
        slide_window=0;

        if(dup_acks==3){
          printf("REENVIAMOS A BASE(recebemos dup e ja sao 3)\n");
          slide_window=2;
        }
      }
   
    }

  }while (base<total);




  // Clean up and exit.
  close(sockfd);
  fclose(file);

  return EXIT_SUCCESS;
}
