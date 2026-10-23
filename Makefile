myshell: command.cc tokenizer.cc command.h tokenizer.h
	g++ command.cc tokenizer.cc -o myshell




## you have command.cc command.h tokenizer.cc tokenizer.h you need compile them and link
## them to one object file called  : "myshell" 
## so we should run make , that will create our object myshell ,
## ./myshell should run your application 