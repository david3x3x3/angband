#include <QtGui>
#include <iostream>
extern "C" {
#include "angband.h"
#include "init.h"
#include "textui.h"
#include "z-util.h"
#include "z-term.h"
}

typedef struct term_data term_data;

struct term_data
{
	term t;
};

static game_command cmd = { CMD_NULL, 0 };

static errr get_cmd_init() {
	std::cout << "get_cmd_init()\n";
	cmd.command = CMD_NEWGAME;
    cmd_insert_s(&cmd);
	return 0;
}

static errr qt_get_cmd(cmd_context context, bool wait)
{
	std::cout << "qt_get_cmd()\n";
	if (context == CMD_INIT) 
		return get_cmd_init();
	else 
		return textui_get_cmd(context, wait);
}

class AngbandTextEvent : public QEvent {
public:
	AngbandTextEvent() : QEvent((QEvent::Type)(QEvent::User + 1)) {}
	~AngbandTextEvent() {}
	int x;
	int y;
	int n;
	byte a;
	const wchar_t *s;
};

class AngbandApp : public QApplication {
public:
	AngbandApp( int argc, char **argv ) : QApplication(argc, argv) {
		mainWindow = new QMainWindow();
		scene = new QGraphicsScene(QRectF(0, 0, 80*8, 24*10));
		scene->setBackgroundBrush(Qt::black);
		QGraphicsView view(scene);
		QFont myfont("Courier", 10);
		QFontInfo myfontinfo(myfont);
		qreal height = 0.0;
		qreal width = 0.0;
		char str[2];
		for (int r=0;r<24;r++) {
			for (int c=0;c<80;c++) {
				sprintf(str, "%c", ' '+((r*24+c)%224));
				QGraphicsTextItem *text = scene->addText(str);
				text->setFont(myfont);
				text->document()->setDocumentMargin(0);
				text->setDefaultTextColor(Qt::white);
				if (!r && !c) {
					QRectF rect = text->boundingRect();
					height = text->document()->size().height();
					width = text->document()->size().width();
					scene->setSceneRect(0,0,width*80,height*24);
					std::cout << "width = " << width << "\n";
				}
				text->translate(c*width,r*height);
				screen[r][c] = text;
			}
		}
		mainWindow->setCentralWidget(&view);
		//app->setMainWidget( view );
		mainWindow->show();

		cmd_get_hook = qt_get_cmd;
	}

	errr term_text_qt(int x, int y, int n, byte a, const wchar_t *s) {
		std::cout << "Term_text_qt(" << s << ")\n";
		/* Draw the text */
		/* Infoclr_set(clr[a]); */
		char str[2];
		for (int i=0; i< n; i++) {
			sprintf(str, "%c", s[i] < 256 ? s[i] : '*');
			screen[y][x+i]->setPlainText(str);
		}

		/* Success */
		return (0);
	}

	void QObject::customEvent(QEvent *event) {
		if(e->type() == (QEvent::User + 1)) {
			std::cout "custom event received\n";
		}
	}

private:
	QGraphicsTextItem *screen[24][80];
	QApplication *app;
	QMainWindow *mainWindow;
	QGraphicsScene *scene;
} *app;

static errr term_text_qt(int x, int y, int n, byte a, const wchar_t *s) {
	return app->term_text_qt(x,y,n,a,s);
}

int init_qt( int argc, char **argv ) {
	app = new AngbandApp( argc, argv );
	return 0;
}

/*
 * Init some stuff
 *
 * This function is used to keep the "path" variable off the stack.
 */
static void init_stuff(void)
{
	char path[1024];

	/* Prepare the path XXX XXX XXX */
	/* This must in some way prepare the "path" variable */
	/* so that it points at the "lib" directory.  Every */
	/* machine handles this in a different way... */
	my_strcpy(path, "lib/", sizeof(path));

	/* Prepare the filepaths */
	init_file_paths(path, path, path);

	/* Set up sound hook */
	sound_hook = NULL;

	/* Set up the display handlers and things. */
	init_display();
}

static void term_init_qt(term *t) {
	std::cout << "term_init_qt()\n";
}

static void term_nuke_qt(term *t) {
	std::cout << "term_nuke_qt()\n";
}

static errr term_xtra_qt(int n, int v) {
	std::cout << "term_xtra_qt(" << n << "," << v << ")\n";
	return -1;
}

static errr term_wipe_qt(int x, int y, int n) {
	std::cout << "term_wipe_qt(" << x << "," << y << "," << n <<")\n";
	return -1;
}

static errr term_curs_qt(int x, int y) {
	std::cout << "term_curs_qt(" << x << "," << y << ")\n";
	return -1;
}

static errr term_pict_qt(int x, int y, int n, const byte *ap,
						 const wchar_t *cp, const byte *tap,
						 const wchar_t *tcp) {
	std::cout << "term_pict_qt()\n";
	return -1;
}

static errr term_data_init(term_data *td, int i) {
	term *t = &td->t;
	term_init(t, 80, 24, i);

    /* Prepare the init/nuke hooks */
    t->init_hook = term_init_qt;
    t->nuke_hook = term_nuke_qt;
    
    /* Prepare the function hooks */
    t->xtra_hook = term_xtra_qt;
    t->wipe_hook = term_wipe_qt;
    t->curs_hook = term_curs_qt;
    t->text_hook = term_text_qt;
    t->pict_hook = term_pict_qt;
    
	angband_term[i] = t;
	return 0;
}

class GameThread : public QThread
{
	Q_OBJECT

	protected:
	void run();
};

void GameThread::run() {
	std::cout << "starting game thread\n";
	/* Play the game */
	play_game();

	/* Free resources */
	cleanup_angband();
}

int main( int argc, char **argv ) {
	if (init_qt(argc, argv) != 0) {
		//quit("Oops!");
		exit(1);
	}

	/* Initialize some stuff */
	init_stuff();

	term_data td;
	term_data_init(&td, 0);
	Term_activate(&td.t);

	GameThread *game_thread = new GameThread();
	// QObject::connect( game_thread, SIGNAL( MySignal() ),
	// 				  & myObject, SLOT( MySlot() ) );

	game_thread->start();

	return app->exec();
}

#include "main-qt.moc"
