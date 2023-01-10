#include <ctype.h>

#include "send_packet.h"

#define BUFSIZE 1460

// sett til 1 for å unngå pakketap ved oppstart
int garanter_reg = 0;

// sekvensnummer for sendte pakker
int server_sekvens_nr = 0;

// Holder på nicken tilhørende adressen i dest_addr
char *cache_nick = NULL;

char *mitt_nick;

int timeout_sek;

// holder på tiden for siste sendingsforsøk
// dersom denne er -1, vil det si at en ny melding kan sendes.
int sist_sendt = -1;

// struct som brukes i fire lenkelister: blokkeringsliste,
// holde på sekvensnummer til og fra, samt holde på usendte meldinger
struct bruker {
    char *nick;
    int sekvens_nr;
    struct bruker *neste;
    struct melding *koe;
};

// struct for usendt melding
struct melding {
    char *tekst;
    int ant_forsoek;
    struct melding *neste;
};

// kø for meldinger som venter på å bli sendt
struct bruker *meldingskoe = NULL;

struct bruker *blocked_liste = NULL;

void rydd();

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

// print-metode for testing
void print_blocked() {

    struct bruker *liste = blocked_liste;

    while (liste != NULL) {
        printf("%s\n", liste->nick);
        liste = liste->neste;
    }
    printf("Slutt\n");
}

// sjekker om en nick er blokkert
int er_blocked(char *nick) {

    struct bruker *liste = blocked_liste;

    while (liste != NULL) {
        if (strcmp(liste->nick, nick) == 0) {
            return 1;
        }
        liste = liste->neste;
    }
    return 0;
}

void block(char *nick) {

    if (er_blocked(nick) == 0) {

        struct bruker *b = malloc(sizeof(struct bruker));
        sjekk_malloc(b, "block");

        b->nick = strdup(nick);
        b->neste = blocked_liste;
        blocked_liste = b;
        
    }
    printf("'%s' har blitt blokkert.\n", nick);
}

void unblock(char *nick) {

    struct bruker *liste = blocked_liste;
    struct bruker *forrige;

    if (strcmp(liste->nick, nick) == 0) {

        blocked_liste = liste->neste;
        free(liste->nick);
        free(liste);

    } else {
        while (liste != NULL) {
            if (strcmp(liste->nick, nick) == 0) {
                forrige->neste = liste->neste;

                free(liste->nick);
                free(liste);

                liste = forrige;
            }
            forrige = liste;
            liste = liste->neste;
        }
    }
    printf("'%s' er ikke lenger blokkert.\n", nick);
}

// holder på forventede sekvensnummere for mottatte meldinger 
struct bruker *mottatt = NULL;

// sekvensnummere for sendte meldinger
struct bruker *sendt = NULL;

// returnerer 1 hvis sekvensnummeret er som forventet, 
// 0 hvis meldingen er duplikat
int forventet_sekv(char *nick, int sekv) {

    struct bruker *liste = mottatt;

    while (liste != NULL) {
        if (strcmp(liste->nick, nick) == 0) {

            if (sekv == liste->sekvens_nr) {
                liste->sekvens_nr = 1 - liste->sekvens_nr;
                return 1;
            } else {
                return 0;
            }
        }
        liste = liste->neste;
    }

    struct bruker *b = malloc(sizeof(struct bruker));
    sjekk_malloc(b, "forventet_sekv");
    b->nick = strdup(nick);
    b->neste = mottatt;
    b->sekvens_nr = 1 - sekv;
    mottatt = b;

    return 1;
}

// henter sekvensnummeret som skal brukes i melding til nick
// bytter sekvensnummer hver gang det blir hentet ut.
int hent_sekv(char *nick) {

    struct bruker *liste = sendt;

    while (liste != NULL) {
        if (strcmp(liste->nick, nick) == 0) {
            liste->sekvens_nr = 1 - liste->sekvens_nr;
            return liste->sekvens_nr;
        }
        liste = liste->neste;
    }

    struct bruker *b = malloc(sizeof(struct bruker));
    sjekk_malloc(b, "hent_sekv");
    b->nick = strdup(nick);
    b->neste = sendt;
    b->sekvens_nr = 0;
    sendt = b;

    return b->sekvens_nr;
}

// fjerner alle planlagte meldinger til en nick dersom brukeren ikke kan nås.
void fjern_alle() {

    struct bruker *liste = meldingskoe;
    struct melding *melding, *temp;
    struct bruker *forrige_bruker = NULL;

    while (liste->neste) {
        forrige_bruker = liste;
        liste = liste->neste;
    }

    if (forrige_bruker == NULL) {
        meldingskoe = NULL;
    } else {
        forrige_bruker->neste = NULL;
    }

    melding = liste->koe;

    while(melding) {
        temp = melding;
        melding = melding->neste;
        free(temp->tekst);
        free(temp);
    }

    free(liste->nick);
    free(liste);
}

// fjerner melding fra køen etter håndtering
void fjern_siste() {
    struct bruker *liste = meldingskoe;
    struct melding *melding;
    struct melding *forrige_melding = NULL;
    struct bruker *forrige_bruker = NULL;

    while (liste->neste) {
        forrige_bruker = liste;
        liste = liste->neste;
    }
    melding = liste->koe;

    while (melding->neste) {
        forrige_melding = melding;
        melding = melding->neste;
    }

    free(melding->tekst);
    free(melding);

    if (forrige_melding == NULL) {
        free(liste->nick);
        free(liste);

        if (forrige_bruker == NULL) {
            meldingskoe = NULL;
        } else {
            forrige_bruker->neste = NULL;
        }
    } else {
        forrige_melding->neste = NULL;
    }
}

void rydd() {

    struct bruker *temp;
    
    while (blocked_liste) {
        free(blocked_liste->nick);
        temp = blocked_liste;
        blocked_liste = blocked_liste->neste;
        free(temp);
    }

    while (mottatt) {
        free(mottatt->nick);
        temp = mottatt;
        mottatt = mottatt->neste;
        free(temp);
    }

    while (sendt) {
        free(sendt->nick);
        temp = sendt;
        sendt = sendt->neste;
        free(temp);
    }

    while (meldingskoe) {
        fjern_alle();
    }

    free(mitt_nick);
    free(cache_nick);
}

// Sjekker lengde og whitespace på nick
void sjekk_nick() {

    if (strlen(mitt_nick) > 20) {
        printf("Kallenavnet kan maksimalt ha 20 tegn.\n");
        exit(EXIT_SUCCESS);
    }
    if (strlen(mitt_nick) < 1) {
        printf("Kallenavnet må minst ha 1 tegn.\n");
        exit(EXIT_SUCCESS);
    }
    for (unsigned int i = 0; i < strlen(mitt_nick); i++) {
        if (isspace(mitt_nick[i]) != 0) {
            printf("Kallenavnet kan ikke inneholde whitespace.\n");
            exit(EXIT_SUCCESS);
        }

        if (mitt_nick[i] < 0) {
            printf("Kallenavnet kan kun inneholde ASCII tegn.\n");
            exit(EXIT_SUCCESS);
        }
    }
}

struct sockaddr_in dest_addr, server_addr;
int fd;

// sender REG melding til server
void send_registrering() {

    int rc;
    char *melding = malloc(sizeof(char) * 30);
    sjekk_malloc(melding, "send_registrering");

    sprintf(melding, "PKT %d REG %s", server_sekvens_nr, mitt_nick);

    if (garanter_reg) {
        rc  = sendto(fd, melding, strlen(melding), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    } else {
        rc = send_packet(fd, melding, strlen(melding), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
    }
    sjekk_error(rc, "send_packet");

    free(melding);
}

// Håndterer registrering og sjekker ACK
void registrer() {

    int rc, svar_sekvens_nr;
    char buf[32];
    fd_set fds;

    // settes til 1 når bruker er registrert
    int suksess = 0;
    // Antall registreringsforsøk
    int ant_reg = 0;
    
    struct timeval timeout;
    timeout.tv_sec = timeout_sek;
    timeout.tv_usec = 0;

    // forsøker å registrere 3 ganger
    
    while (suksess != 1 && ant_reg < 3)  {

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        send_registrering();

        rc = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
        sjekk_error(rc, "select");

        if (rc == 0) {
            fprintf(stderr, "Klarte ikke å registrere før tidsavbrudd. Prøver på nytt...\n");
            ant_reg++;
            timeout.tv_sec = timeout_sek;
        }

        if (FD_ISSET(fd, &fds)) {
            // motta svar, sjekk at OK (sjekke med recvfrom at avsender = server?)
            rc = recv(fd, buf, 31, 0);
            sjekk_error(rc, "recv");
            buf[rc] = 0;

            rc = sscanf(buf, "ACK %d OK", &svar_sekvens_nr);

            if (rc != 1 || svar_sekvens_nr != server_sekvens_nr) {
                fprintf(stderr, "Uventet svar. Prøver på nytt...\n");
                ant_reg++;
            }
            printf("Bruker '%s' har blitt registrert.\n", mitt_nick);
            suksess = 1;
        }

    }

    if (suksess == 0) {
        fprintf(stderr, "Registrering feilet. Avslutter.\n");
        rydd();
        exit(EXIT_SUCCESS);
    }
}

// sender LOOKUP og lagrer info fra responsen i 'dest_addr'
// returnerer -1 dersom nick ikke er registrert.
int oppslag(char *nick) {

    server_sekvens_nr = 1 - server_sekvens_nr;

    int svar_sekvens_nr, port_nr;
    char *recv_nick = malloc(sizeof(char) * 21);
    char *addr = malloc(sizeof(char) * 16);
    
    sjekk_malloc(recv_nick, "oppslag");
    sjekk_malloc(addr, "oppslag");

    int rc;
    char *melding = malloc(sizeof(char) * 33);
    sjekk_malloc(melding, "oppslag");
    char buf[64];

    struct timeval timeout;
    timeout.tv_sec = timeout_sek;
    timeout.tv_usec = 0;

    int suksess = 0;
    int ant_oppslag = 0;

    fd_set fds;

    sprintf(melding, "PKT %d LOOKUP %s", server_sekvens_nr, nick);

    // forsøker 3 ganger
    while (suksess != 1 && ant_oppslag < 3) {

        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        rc = send_packet(fd, melding, strlen(melding), 0, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
        sjekk_error(rc, "sendto");

        rc = select(FD_SETSIZE, &fds, NULL, NULL, &timeout);
        sjekk_error(rc, "select");

        if (rc == 0) {
            ant_oppslag++;
            timeout.tv_sec = timeout_sek;
        }

        if (FD_ISSET(fd, &fds)) {
            rc = recv(fd, buf, 63, 0);
            sjekk_error(rc, "recv");
            buf[rc] = 0;

            rc = sscanf(buf, "ACK %d NICK %s IP %s PORT %d", &svar_sekvens_nr, recv_nick, addr, &port_nr);
            if (rc == 4 && svar_sekvens_nr == server_sekvens_nr) {
                suksess = 1;
                continue;
            }

            rc = sscanf(buf, "ACK %d NOT FOUND", &svar_sekvens_nr);
            if (rc == 1 && svar_sekvens_nr == server_sekvens_nr) {
                free(recv_nick);
                free(addr);
                free(melding);
                return -1;
            }
        }
    }

    free(melding);

    if (suksess == 0) {
            fprintf(stderr, "Oppslag feilet. Avslutter.\n");
            free(recv_nick);
            free(addr);
            rydd();
            exit(EXIT_SUCCESS);
        }

    // Innlesning
    
    if (rc != 4 || svar_sekvens_nr != server_sekvens_nr) {
        free(recv_nick);
        free(addr);
        return -1;
    }

    // info lagres i dest_addr
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port_nr);

    rc = inet_pton(AF_INET, addr, &dest_addr.sin_addr.s_addr);
    sjekk_error(rc, "inet_pton");
    if (rc == 0) {
        fprintf(stderr, "Ugyldig IP adresse: %s", addr);
        free(recv_nick);
        free(addr);
        rydd();
        exit(EXIT_FAILURE);
    }

    free(recv_nick);
    free(addr);

    return 0;
}

// melding legges i køen
void lagre_melding(char *buf) {

    int rc;
    char *tekst;
    char *nick = malloc(sizeof(char) * 21);
    char *melding = malloc(sizeof(char) * BUFSIZE);

    sjekk_malloc(nick, "lagre_melding");
    sjekk_malloc(melding, "lagre_melding");

    rc = sscanf(buf, "@%20s ", nick);
    if (rc != 1) {
        printf("Korrekt bruk: @<mottaker> <tekst> eller <BLOCK/UNBLOCK> <nick>\n");
        free(nick);
        free(melding);
        return;
    }
    if (er_blocked(nick) == 1) {
        free(nick);
        free(melding);
        return;
    }

    int sekvens_nr = hent_sekv(nick); 
    // Setter 'tekst' til resten av bufferet
    tekst = &buf[strlen(nick) + 2];
    tekst[strlen(tekst)] = 0;

    sprintf(melding, "PKT %d FROM %s TO %s MSG %s", sekvens_nr, mitt_nick, nick, tekst);

    struct bruker *liste = meldingskoe;
    
    while (liste != NULL && strcmp(liste->nick, nick) != 0) {
        liste = liste->neste;
    }

    if (liste == NULL) {
        liste = malloc(sizeof(struct bruker));
        liste->nick = nick;
        liste->koe = NULL;
        liste->neste = meldingskoe;
        meldingskoe = liste;
    }

    struct melding *pakke = malloc(sizeof(struct melding));
    sjekk_malloc(pakke, "lagre_melding");

    pakke->tekst = melding;
    pakke->ant_forsoek = 0;

    pakke->neste = liste->koe;
    liste->koe = pakke;
}

// sender melding og gjør oppslag dersom nick ikke er cached
int send_melding(char *melding, char *nick) {

    int rc;
    
    if (cache_nick != NULL) {
        if (strcmp(nick, cache_nick) != 0) {
            rc = oppslag(nick);

            if (rc == -1) {
                fprintf(stderr, "NICK %s NOT REGISTERED\n", nick);
                return -1;
            }
            free(cache_nick);
            cache_nick = strdup(nick);
        }
    } else {
        rc = oppslag(nick);

        if (rc == -1) {
            fprintf(stderr, "NICK %s NOT REGISTERED\n", nick);
            return -1;
        }
        cache_nick = strdup(nick);
    }
    rc = send_packet(fd, melding, strlen(melding), 0, (struct sockaddr*) &dest_addr, sizeof(struct sockaddr_in));
    sjekk_error(rc, "send_packet");

    return 0;
}

// finner neste melding i køen og en kaller 'send_melding()'
void send_neste(int tid) {

    int rc;
    char *nick;
    struct bruker *liste = meldingskoe;
    struct melding *melding;

    if (liste == NULL) {
        return;
    }

    while (liste->neste) {
        liste = liste->neste;
    }

    nick = liste->nick;
    melding = liste->koe;

    while (melding->neste) {
        melding = melding->neste;
    }

    // nytt oppslag
    if (melding->ant_forsoek == 2) {
        rc = oppslag(nick);

        if (rc == -1) {
            fprintf(stderr, "NICK %s NOT REGISTERED\n", nick);
            fjern_alle();
            sist_sendt = -1;
            return;
        }
    }

    if (melding->ant_forsoek == 4) {
        fprintf(stderr, "NICK %s UNREACHABLE\n", nick);
        fjern_alle();
        sist_sendt = -1;
        return;
    }

    rc = send_melding(melding->tekst, nick);
    if (rc == -1) {
        fjern_alle();
        return;
    }
    melding->ant_forsoek += 1;
    sist_sendt = tid;
}

void motta_melding(char* buf, struct sockaddr_in *avsender) {
    int rc;
    char *melding, til_nick[21], fra_nick[21];
    int sekv_nr;

    // if-sjekk ser bort ifra ACKs fra hjerteslag
    if (sscanf(buf, "ACK %d OK", &sekv_nr) == 1) {

        // dersom avsender av ACK matcher dest_addr,
        // regnes sist-sendte melding som mottatt
        if (avsender->sin_port != dest_addr.sin_port) {
            return;
        }
        if (avsender->sin_addr.s_addr != dest_addr.sin_addr.s_addr) {
            return;
        }
        fjern_siste();
        sist_sendt = -1;
        return;
    }
    // håndtering av mottatt tekstmelding
    rc = sscanf(buf, "PKT %d FROM %s TO %s MSG ", &sekv_nr, fra_nick, til_nick);

    // sjekker at formatet og mottaker stemmer
    if (rc != 3) {
        fprintf(stderr, "FEIL: Mottatt melding på feil format.\n");
        return;
    }
    if (strcmp(til_nick, mitt_nick)) {
        fprintf(stderr, "FEIL: Mottatt melding med feil navn.\n");
        return;
    }
    
    // sjekker at avsender ikke er blokkert
    if (er_blocked(fra_nick) == 0 && forventet_sekv(fra_nick, sekv_nr)) {

        melding = &buf[strlen(fra_nick) + strlen(til_nick) + 20];
        printf("%s: %s\n", fra_nick, melding);
    }

    // sender ACK
    char *respons = malloc(sizeof(char) * 32);
    sjekk_malloc(respons, "motta_melding");

    sprintf(respons, "ACK %d OK", sekv_nr);
    send_packet(fd, respons, strlen(respons), 0, (struct sockaddr*) avsender, sizeof(struct sockaddr_in));

    free(respons);
}

int main(int argc, char const *argv[]) {

    if (argc != 6) {
        printf("Korrekt bruk: ./upush_client <nick> <adresse> <port> <timeout> <tapssansynlighet>\n");
        return EXIT_SUCCESS;
    }
    set_loss_probability(((float) atoi(argv[5]))/100);

    mitt_nick = strdup(argv[1]);
    const char *addr = argv[2];
    int port_nr = atoi(argv[3]);
    timeout_sek = atoi(argv[4]);

    sjekk_nick();

    int rc;

    struct in_addr server_ip;
    socklen_t addr_len = sizeof(struct sockaddr_in);

    rc = inet_pton(AF_INET, addr, &server_ip.s_addr);
    sjekk_error(rc, "inet_pton");
    if (!rc) {
        fprintf(stderr, "Ugyldig IP adresse\n");
        return EXIT_FAILURE;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_nr);
    server_addr.sin_addr = server_ip;

    struct sockaddr_in min_addr, avsender;

    min_addr.sin_family = AF_INET;
    min_addr.sin_port = 0;
    min_addr.sin_addr.s_addr = INADDR_ANY;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    sjekk_error(fd, "socket");

    rc = bind(fd, (struct sockaddr*) &min_addr, sizeof(struct sockaddr_in));
    sjekk_error(rc, "Denne porten kan ikke brukes av følgende grunn");

    char buf[BUFSIZE];
    fd_set fds;

    registrer(timeout_sek);

    // Hovedhendelsesløkke
    struct timeval tick;
    tick.tv_sec = 1;
    tick.tv_usec = 0;

    char block_nick[21];

    int ant_sek = 0;

    printf("\nSkriv QUIT for å avslutte.\n\n");

    buf[0] = 0;
    while (strcmp("QUIT", buf)) {

        FD_ZERO(&fds);

        FD_SET(fd, &fds);
        FD_SET(STDIN_FILENO, &fds);

        rc = select(FD_SETSIZE, &fds, NULL, NULL, &tick);
        sjekk_error(rc, "select");

        // håndtering av meldinger hvert sekund
        if (rc == 0) {
            tick.tv_sec = 1;
            ant_sek++;

            // Hjerteslag hvert 10 sekund
            if (ant_sek % 10 == 0) {
                server_sekvens_nr = 1 - server_sekvens_nr;
                send_registrering();
            }

            if (sist_sendt == -1 || ant_sek - sist_sendt >= timeout_sek) {
                send_neste(ant_sek);
            }
            continue;
        }

        if (FD_ISSET(fd, &fds)) {
            //melding mottatt over nettet
            rc = recvfrom(fd, buf, BUFSIZE - 1, 0, (struct sockaddr*) &avsender, &addr_len);
            sjekk_error(rc, "recvfrom");
            buf[rc] = 0;
            motta_melding(buf, &avsender);
        }

        if (FD_ISSET(STDIN_FILENO, &fds)) {
            hent_string(buf, BUFSIZE);

            if (sscanf(buf, "BLOCK %s", block_nick) == 1) {
                block(block_nick);
                continue;
            }
            if (sscanf(buf, "UNBLOCK %s", block_nick) == 1) {
                unblock(block_nick);
                continue;
            }
            if (strcmp("QUIT", buf) != 0) {

                lagre_melding(buf);
            }
        }
    }

    rydd();

    close(fd);

    return EXIT_SUCCESS;
}
