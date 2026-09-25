# Copyright (C) 2026 Qore Technologies, s.r.o.
%modern

File f();
f.open2(ARGV[0], O_CREAT | O_TRUNC | O_WRONLY);
f.write("data\n");
f.write(stdin.read(-1) ?? "");
print("print\n");
printf("printf\n");
stdout.write("stdout\n");
stderr.write("stderr\n");
f.close();
