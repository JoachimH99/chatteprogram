#include "send_packet.h"

#define BUFSIZE 64

void rydd();

struct klient {
    char *nick;
    char *addr;
    int port_nr;
    int sekvens_nr;
    struct klient *neste;
    struct timeval *siste_reg;
};

void hent_string(char buf[], int size) {
    char c;

    fgets(buf, size, stdin);
    if (buf[strlen(buf) - 1] == '\n') {
        buf[strlen(buf) - 1] = 0;
    } else {
        while ((c = getchar()) != '\n' && c != EOF);
    }
}

void sjekk_error(int resultat, char *melding) {
        
    if (resultat == -1) {
        perror(melding);
        rydd();
        exit(EXIT_FAILURE);
    }
}

void sjekk_malloc(void *p, char *melding) {

    if (p == NULL) {
        fprintf(stderr, "Malloc feil: %s", melding);
        rydd();
        exit(EXIT_FAILURE);
    }
}

// Returnerer klient-info i string format, eller NOT FOUND
char* klient_til_string(struct klient *k) {

    char *respons = malloc(sizeof(char) * 64);
    sjekk_malloc(respons, "klient_til_string");

    if (k == NULL) {
        sprintf(respons, "NOT FOUND");
        return respons;
    }

    sprintf(respons, 
            "NICK %s IP %s PORT %d", 
            k->nick, 
            k->addr, 
            k->port_nr);

    return respons;
}

// metode for testing
void print_liste(struct klient *liste) {
    char *klient;

    while (liste != NULL) {

        klient = klient_til_string(liste);
        printf("%s SISTE REG %ld\n", klient, liste->siste_reg->tv_sec);
        liste = liste->neste;
        free(klient);
    }
    printf("Slutt\n\n");
}


char* lag_respons(int sekvens_nr, char *melding) {

    char *respons = malloc(sizeof(char) * BUFSIZE);
    sjekk_malloc(respons, "lag_respons");

    sprintf(respons, "ACK %d %s", sekvens_nr, melding);

    return respons;
}

// Lenkeliste av registrerte klienter
struct klient *klienter = NULL;

// Oppslag, returnerer NULL ved treff eldre enn 30 sek
struct klient* finn_klient(char *nick) {
    
    struct klient *temp = klienter;
    struct timeval naa, diff;

    gettimeofday(&naa, 0);

    while (temp != NULL) {
        if (strcmp(nick, temp->nick) == 0) {
            timersub(&naa, temp->siste_reg, &diff);
            
            if (diff.tv_sec <= 30) {
                return temp;
            }
            return NULL;
        }
        temp = temp->neste;
    }
    return NULL;
}

// Lagrer klient i lenkelisten
void lagre_klient(char *addr, int port_nr, char *nick, int sekvens_nr) {

    // Sjekker om navnet er tatt
    struct klient *k = finn_klient(nick);

    // Dersom navnet er tatt oppdateres info
    if (k != NULL) {
        free(k->addr);
        k->addr = strdup(addr);
        k->port_nr = port_nr;
        k->sekvens_nr = sekvens_nr;
        gettimeofday(k->siste_reg, NULL);
        return;
    }

    k = malloc(sizeof(struct klient));
    sjekk_malloc(k, "lagre_klient");

    k->nick = strdup(nick);
    k->addr = strdup(addr);

    if (k->nick == NULL || k->addr == NULL) {
        perror("strdup");
        exit(EXIT_FAILURE);
    }
    k->port_nr = port_nr;
    k->neste = klienter;
    klienter = k;

    k->siste_reg = malloc(sizeof(struct timeval));
    sjekk_malloc(k->siste_reg, "lagre_klient");

    gettimeofday(k->siste_reg, NULL);
}

// Ektraherer adresse og port, og kaller lagre_klient()
void registrer_klient(char *nick, struct sockaddr *klient_addr, int sekvens_nr) {

    struct sockaddr_in *klient = (struct sockaddr_in *) klient_addr;

    char *addr = inet_ntoa(klient->sin_addr);
    int port_nr = ntohs(klient->sin_port);

    lagre_klient(addr, port_nr, nick, sekvens_nr);
}

// Fjerner klienter som er eldre enn 30 sek
void fjern_gamle() {

    struct klient *klient = klienter;
    struct klient *forrige = NULL;
    struct klient *neste;
    struct timeval naa, diff;

    gettimeofday(&naa, NULL);

    while (klient != NULL) {
        timersub(&naa, klient->siste_reg, &diff);
        
        neste = klient->neste;

        if (diff.tv_sec > 30) {
            if (forrige == NULL) {
                free(klient->addr);
                free(klient->nick);
                free(klient->siste_reg);

                klienter = klient->neste;
                free(klient);

            } else {
                free(klient->addr);
                free(klient->nick);
                free(klient->siste_reg);

                forrige->neste = klient->neste;
                free(klient);
            }
        } else {
            forrige = klient;
        }
        klient = neste;
    }
}

// Leser melding, håndterer returnerer svar
char* les(char *buf, struct sockaddr *klient_addr) {

    char *type = malloc(sizeof(char) * 4); // PKT
    sjekk_malloc(type, "les");

    int sekvens_nr;
    char *instruks = malloc(sizeof(char) * 7); // REG eller LOOKUP
    sjekk_malloc(instruks, "les");

    char *nick = malloc(sizeof(char) * 21);
    sjekk_malloc(nick, "les");

    char *respons, *klient;

    int rc;
    rc = sscanf(buf, "%3s %d %6s %20s", type, &sekvens_nr, instruks, nick);
    if (rc != 4) {
        respons = lag_respons(0, "WRONG FORMAT");
        free(type);
        free(instruks);
        return respons;
    }

    if (strcmp(type, "PKT") != 0) {
        respons = lag_respons(sekvens_nr, "WRONG FORMAT");
        free(type);
        free(instruks);
        free(nick);
        return respons;
    }

    if (strcmp(instruks, "LOOKUP") == 0) {
        klient = klient_til_string(finn_klient(nick));
        respons = lag_respons(sekvens_nr, klient);
        free(klient);

    } else if (strcmp(instruks, "REG") == 0) {
        registrer_klient(nick, klient_addr, sekvens_nr);
        respons = lag_respons(sekvens_nr, "OK");

    } else {
        respons = lag_respons(sekvens_nr, "WRONG FORMAT");
    }
    free(type);
    free(instruks);
    free(nick);
    free(buf);
    return respons;
}

void rydd() {

    struct klient *temp;
    
    while (klienter) {

        free(klienter->addr);
        free(klienter->nick);
        free(klienter->siste_reg);

        temp = klienter;
        klienter = klienter->neste;

        free(temp);
    }
}

int main(int argc, char const *argv[]) {

    if (argc != 3) {
        printf("Korrekt bruk: ./upush_server <port> <tapssansynlighet>\n");
        return EXIT_SUCCESS;
    }  
    set_loss_probability(((float) atoi(argv[2]))/100);

    int port_nr, fd, rc;
    struct sockaddr_in min_addr, klient_addr;
    char buf[BUFSIZE];
    socklen_t addr_len = sizeof(struct sockaddr_in);

    port_nr = atoi(argv[1]);

    min_addr.sin_family = AF_INET;
    min_addr.sin_port = htons(port_nr);
    min_addr.sin_addr.s_addr = INADDR_ANY;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    sjekk_error(fd, "socket");

    rc = bind(fd, (struct sockaddr*) &min_addr, sizeof(struct sockaddr_in));
    sjekk_error(rc, "Denne porten kan ikke brukes av følgende grunn");

    char *respons;
    fd_set fds;

    struct timeval hjerteslag;
    hjerteslag.tv_sec = 30;
    hjerteslag.tv_usec = 0;

    buf[0] = 0;
    printf("Skriv 'QUIT' for å avslutte serveren.\n\n");
    while (strcmp("QUIT", buf)) {

        FD_ZERO(&fds);

        FD_SET(fd, &fds);
        FD_SET(STDIN_FILENO, &fds);

        rc = select(FD_SETSIZE, &fds, NULL, NULL, &hjerteslag);
        sjekk_error(rc, "select");

        if (rc == 0) {
            fjern_gamle();
            hjerteslag.tv_sec = 30;
            continue;
        }

        if (FD_ISSET(fd, &fds)) {
            // Motta
            rc = recvfrom(fd, buf, BUFSIZE - 1, 0, (struct sockaddr*) &klient_addr, &addr_len);
            sjekk_error(rc, "recvfrom");
            buf[rc] = 0;

            // Håndtere
            respons = les(strdup(buf), (struct sockaddr*) &klient_addr);

            // Returnere 
            rc = send_packet(fd, respons, strlen(respons), 0, (struct sockaddr*) &klient_addr, sizeof(struct sockaddr_in));
            sjekk_error(rc, "send_packet");

            free(respons);
        }
        
        if (FD_ISSET(STDIN_FILENO, &fds)) {
            hent_string(buf, BUFSIZE);
        }
    }

    rydd();

    close(fd);

    return EXIT_SUCCESS;
}

