/*!
 * \brief	Данная программа создаем 4 процесса-потомка, каждый из которых
 *			читает по строке стихотворения по следующему правилу:
 *			1-ый потомок читает 1-ую, 5-ую, 9-ую строки и т. д. ,
 *			2-ой потомок читает 2-ую, 6-ую, 10-ую строки и т. д.,
 *			3-ий и 4-ый потомки делают соответствующие действия.
 *			Главный процесс выводит прочитанное стихотворение на экран.
 *
 * \author	Рогоза А. А.
 * \author	Романов С. А.
 * \date	13/04/2016
 */
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>

#define MSG_SIZE 150

//! Сообщение
struct message {
	long mtype;			  //!< Тип сообщения
	char mtext[MSG_SIZE]; //!< Текст сообщения
};

//! Семафоры
union sem {
	int val;
	struct semid_ds * buf;
	unsigned short * array;
};

//! Общие переменные для всех процессов
int	poemFileId;
int msgId;
int semId;

/*!
 * \brief Процедура для закрытия набора семафоров и очереди сообщений
 */
void closeIpc()
{
	msgctl( msgId, IPC_RMID, 0 );
	semctl( semId, IPC_RMID, 0 );
}

/*! @brief Обработка ошибки
 *  @param buf текст ошибки
 */
void error( const char * buf )
{
	closeIpc();
	printf( "ERROR: %s\n", buf );
	exit( 1 );
}

//! Выполнение функции p() над семафором num
void p( int semid, int num )
{
	struct sembuf p_buf = { num, -1, 0 };
	if ( semop( semid, &p_buf, 1 ) == -1 ) {
		printf( "error in p %d\n", semop( semid, &p_buf, 1 ) );
		exit( 1 );
	}
}

//! Выполнение функции v() над семафором num
void v( int semid, int num )
{
	struct sembuf p_buf = { num, 1, 0 };
	if ( semop( semid, &p_buf, 1 ) == -1 ) {
		printf( "error in v %d\n", semop( semid, &p_buf, 1 ) );
		exit( 1 );
	}
}

/*!
 * \brief Подпрограмма работы процесса
 * \param i		Номер процесса
 * \param semId	Идентификатор набора семафоров
 */
void processWork( int i, int semId )
{
	struct message msg;
	while ( 1 ) {
		msg.mtype = 1;
		p( semId, i ); //! Закрываем доступ к ресурсу для данного процесса
		char text[MSG_SIZE];
		char c = 'a';
		int charCount = 0;
		char isContinue = 1;
		//! Чтение стихотворения
		for ( charCount = 0; c != '\n' && isContinue > 0; ++charCount ) {
			isContinue = read( poemFileId, &c, sizeof( c ) );
			text[charCount] = c;
		}
		text[charCount-1] = '\0';
		sprintf( msg.mtext, "%s", text );

		//! Посылаем сообщение в очередь
		if ( msgsnd( msgId, &msg, sizeof( struct message ), IPC_NOWAIT ) == -1 )
			error( "Can't send a message." );

		v( semId, ( (i + 1) % 4 ) ); //! Открываем доступ к ресурсу для следующего процесса
		if ( !isContinue )
			exit( 0 );
	}
}

int main()
{
	msgId = msgget( IPC_PRIVATE, IPC_CREAT | 0600 );	//! Создание очереди сообщений
	if ( msgId == -1 )
		error( "Can't create the message queue." );
	semId = semget( IPC_PRIVATE, 4, IPC_CREAT | 0600 ); //! Создаем набор из 4-х семафоров
	if ( semId == -1 )
		error( "Can't create the semaphores." );

	union sem semun;   //!< Семафоры

	for ( int i = 0; i < 4; i++ ) {
		semun.val = 0; //! начальное значение семафора
		semctl( semId, i, SETVAL, semun );
	}

	poemFileId = open( "poem.txt", O_RDONLY ); //! открываем файл со стихотворением
	if ( poemFileId == -1 )
		error( "Can't open the file" );

	v( semId, 0 ); //! открываем первый семафор
	for ( int i = 0; i < 3; i++ ) {
		int pid = fork();
		if ( pid == 0 )
			processWork( i, semId );
		else if ( pid == -1 )
			error( "Can't create a process." );
	}

	int pid = fork();
	if ( pid == 0 )
		processWork( 3, semId );
	else if ( pid == -1 )
		error( "Can't create a process." );
	else {
		int status;
		//! Ожидаем завершения работы прцоессов-потомков
		for ( int i = 0; i < 4; ++i )
			wait( &status );

		//! Чтение стихотворения
		struct message msg;
		while ( 1 ) {
			//! Получаем следующую строку из очереди
			if ( msgrcv( msgId, &msg, sizeof( struct message ), 1, IPC_NOWAIT ) == -1 )
				break;
			printf( "%s\n", msg.mtext );
		}
		closeIpc(); //! Закрываем семафоры и очередь
	}
	return 0;
}
