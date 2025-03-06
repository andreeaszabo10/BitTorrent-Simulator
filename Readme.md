Copyright Szabo Cristina-Andreea 2024-2025

# BitTorrent Simulator

In cadrul acestui proiect am implementat protocolul Bittorent folosind MPI.
Astfel, programul are urmatorul flow:
1) fiecare client citeste cate si ce fisiere detine, apoi datele din fisiere, pe care i le transmite trackerului, pentru a stoca o lista cu toate fisierele existente si informatiile despre ele.
2) trackerul primeste informatiile despre fisiere pe rand si le stocheaza, de asemenea creeaza swarm-ul fiecarui fisier si cand primeste de la toti clientii toate fisierele, le trimite un semnal de confirmare
3) apoi clientul porneste thread-urile de download/upload si incep cererile
4) fiecare client se uita de ce fisiere are nevoie si le ia pe rand, intai intreband trackerul despre swarm-ul fisierului respectiv si fiind adaugat la randul lui in swarm, stiind ca o sa primeasca fisierul in curnad
5) trackerul ii raspunde clientului cu swarm-ul fisierului, dar si cu cate chunk-uri are si ce hash-uri au chunk-urile, doar pentru verificare, ca sa se asigure ca a primit ce trebuie clientul
6)  acum ca stie pe cine sa intrebe si cate chunk-uri are de gasit, clientul ia pe rand fiecare alt client din swarm si il intreaba de cate un chunk lipsa din fisier. Pentru eficienta, am folosit algoritmul round robin, astfel incat clientul intreaba de primul chunk lipsa pe primul din swarm, de al doilea pe al doilea client din swarm samd, iar dupa ce l-a intrebat pe ultimul din swarm, revine la primul. De asemenea, am avut grija sa nu il intreb pe clientul care cere. Procesul de cerut chunk-uri continua pana cand am obtinut toate chunk-urile din fisier
7) clientul intrebat verifica daca detine la momentul intrebarii chunk-ul cerut si raspunde cu un mesaj corespunzator: OK daca il are si NO daca nu.
8) daca clientul intrebat are chunk-ul, in lista de fisiere de care are nevoie marchez faptul ca am primit chunk-ul ca sa nu mai intreb pe viitor de el, de asemenea marchez ca acum detin acel chunk si pot sa-l dau mai departe la randul meu. Pentru ca tot primind chunk-uri swarm-urile se mai actualizeaza, o data la 10 cereri cer swarm-ul actualizat al fisierului curent de la tracker, ca sa stiu daca pot intreba mai multe persoane
9) dupa ce am obtinut toate chunk-urile unui fisier de la alti clienti, marchez ca am obtinut un fisier intreg si ca am cu unul mai putin de cerut acum, apoi afisez continutul fisierului tocmai obtinut intr-un fisier de output cu nume corespunzator
10) cand am terminat de cerut toate fisierele, trimit un mesaj catre tracker ca am terminat de cerut tot si inchid executia thread-ului download
11) dupa ce trackerul primeste mesaj de DONE de la toti clientii, trimite thread-urilor upload un mesaj sa incheie si ele executia, apoi se inchide singur, terminandu-se programul 

