# Chatteprogram
Et enkelt chatteprogram kodet i C og bygget på UDP. Hentet fra min besvarelse av hjemmeeksamen i "Introduksjon til operativsystemer og datakommunikasjon". 
Programmet er kun testet over localhost.

## Kjøring
I mappen "chatteprogram", bruk kommandoen "make" for å kompilere.


Serveren må startes først, og kjøres med følgende argumenter:

```./upush_server <port> <tapssansynlighet>```

Klienter kan så kjøres med følgende argumenter:

```./upush_client <nick> <adresse> <port> <timeout> <tapssansynlighet>```

Argumentene "adresse" og "port" i "upush_client" refererer til adressen og porten serveren kjører på. 
Tapssansynlighet er et heltall mellom 0 og 100 som avgjør prosentandel av pakker som går tapt.

### Eksempel

Et enkelt oppsett for kjøring over localhost (kommandoene må kjøres i hver sin terminal):

```./upush_server 5000 0```

```./upush_client alice 127.0.0.1 5000 10 0```

```./upush_client bob 127.0.0.1 5000 10 0```

# Bruk

Klientene kan sende meldinger til hverandre ved å skrive:

```@<nick> <melding>```

Klientene kan også blokkere og avblokkere brukernavn med følgende kommandoer:

```BLOCK <nick>```
```UNBLOCK <nick>```

## Kommentarer
"send_packet.c" og "send_packet.h" er prekodefiler.
