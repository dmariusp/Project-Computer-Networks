Pentru a rula programul, trebuie procedat astfel:
    * din client, se trimit catre server extensiile de intrare, respectiv iesire corespunzatoare conversiei dorite;
    * daca cele doua extensii corespund unei conversii suportate, clientul va introduce apoi path-ul fisierului pe care doreste sa il trimita;
     rezultatul acestei operatii va fi fisierul convertit;

In cazul in care utilitarele pentru convertire nu pot fi rulate (nu figureaza ca fiind instalate pe masina serverului), se vor instala urmatoarele:
    * pdftotext : [poppler/poppler-utils](https://command-not-found.com/pdftotext);
    * ps2pdf/pdf2ps : [ghostscript](https://command-not-found.com/ps2pdf);
    * html2text : [python-html2text](https://command-not-found.com/html2text);
    * latex2html : [latex2html](https://command-not-found.com/latex2html);
    * php2po/html2po : [translate-toolkit](https://command-not-found.com/php2po) (pentru dependenta __phply__: pip install phply).
