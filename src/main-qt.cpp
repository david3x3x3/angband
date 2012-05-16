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
	AngbandTextEvent(int new_x, int new_y, int new_n, int new_a,
					 const wchar_t *s) :
		QEvent((QEvent::Type)(QEvent::User + 1)) {
		x = new_x;
		y = new_y;
		n = new_n;
		a = new_a;
		str = new char[n+1];
		for (int i=0; i < n; i++) {
			wchar_t c = s[i];
			str[i] = c < 256 ? c : '*';
		}
		str[n] = '\0';
	}
	~AngbandTextEvent() {
		delete str;
	}
	int get_x() { return x; }
	int get_y() { return y; }
	int get_n() { return n; }
	int get_a() { return a; }
	char *get_str() { return str; }
private:
	int x;
	int y;
	int n;
	byte a;
	char *str;
};

class AngbandWipeEvent : public QEvent {
public:
	AngbandWipeEvent(int new_x, int new_y, int new_n) :
		QEvent((QEvent::Type)(QEvent::User + 2)) {
		x = new_x;
		y = new_y;
		n = new_n;
	}
	~AngbandWipeEvent() {
	}
	int get_x() { return x; }
	int get_y() { return y; }
	int get_n() { return n; }
private:
	int x;
	int y;
	int n;
};

class AngbandApp : public QApplication {
public:
	AngbandApp( int argc, char **argv ) : QApplication(argc, argv) {
		mainWindow = new QMainWindow();
		scene = new QGraphicsScene(QRectF(0, 0, 80*8, 24*10));
		scene->setBackgroundBrush(Qt::black);
		view = new QGraphicsView(scene);
		QFont myfont("Courier", 10);
		QFontInfo myfontinfo(myfont);
		qreal height = 0.0;
		qreal width = 0.0;
		for (int r=0;r<24;r++) {
			for (int c=0;c<80;c++) {
				QGraphicsTextItem *text = scene->addText(" ");
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
		mainWindow->setCentralWidget(view);
		//app->setMainWidget( view );
		mainWindow->show();

		cmd_get_hook = qt_get_cmd;
	}

	void customEvent(QEvent *e) {
		if(e->type() == (QEvent::User + 1)) {
			std::cout << "text event received\n";
			AngbandTextEvent *te = (AngbandTextEvent *)e;
			char str[2];
			for (int i=0; i< te->get_n(); i++) {
				sprintf(str, "%c", te->get_str()[i]);
				screen[te->get_y()][te->get_x()+i]->setPlainText(str);
			}
		} else if(e->type() == (QEvent::User + 2)) {
			std::cout << "wipe event received\n";
			AngbandWipeEvent *we = (AngbandWipeEvent *)e;
			for (int i=0; i< we->get_n(); i++) {
				screen[we->get_y()][we->get_x()+i]->setPlainText(" ");
			}
		}
	}

private:
	QGraphicsTextItem *screen[24][80];
	QApplication *app;
	QMainWindow *mainWindow;
	QGraphicsScene *scene;
	QGraphicsView *view;
} *app;

static errr term_text_qt(int x, int y, int n, byte a, const wchar_t *s) {
	app->postEvent(app, new AngbandTextEvent(x,y,n,a,s));
	return 0;
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
	switch(n) {
	case TERM_XTRA_EVENT: std::cout << "TERM_XTRA_EVENT\n"; break;
	case TERM_XTRA_FLUSH: std::cout << "TERM_XTRA_FLUSH\n"; break;
	case TERM_XTRA_CLEAR: std::cout << "TERM_XTRA_CLEAR\n"; break;
	case TERM_XTRA_SHAPE: std::cout << "TERM_XTRA_SHAPE\n"; break;
	case TERM_XTRA_FROSH: std::cout << "TERM_XTRA_FROSH\n"; break;
	case TERM_XTRA_FRESH: std::cout << "TERM_XTRA_FRESH\n"; break;
	case TERM_XTRA_NOISE: std::cout << "TERM_XTRA_NOISE\n"; break;
	case TERM_XTRA_BORED: std::cout << "TERM_XTRA_BORED\n"; break;
	case TERM_XTRA_REACT: std::cout << "TERM_XTRA_REACT\n"; break;
	case TERM_XTRA_ALIVE: std::cout << "TERM_XTRA_ALIVE\n"; break;
	case TERM_XTRA_LEVEL: std::cout << "TERM_XTRA_LEVEL\n"; break;
	case TERM_XTRA_DELAY: std::cout << "TERM_XTRA_DELAY\n"; break;
	}
	return -1;
}

static errr term_wipe_qt(int x, int y, int n) {
	std::cout << "term_wipe_qt(" << x << "," << y << "," << n <<")\n";
	app->postEvent(app, new AngbandWipeEvent(x,y,n));
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
	game_thread->start();

	return app->exec();
}

#include "main-qt.moc"
