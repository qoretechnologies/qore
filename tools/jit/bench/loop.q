%modern

int i = 0;
int sum = 0;

while (i < 1000000) {
    sum += i;
    ++i;
}

printf("%d\n", sum);
