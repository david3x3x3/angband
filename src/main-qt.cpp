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

class keyPressCatcher : public QObject {
public:
	keyPressCatcher() {}
	~keyPressCatcher() {}
	bool eventFilter(QObject* object, QEvent* event);
};

class AngApp : public QApplication {
public:
	AngApp( int argc, char **argv ) : QApplication(argc, argv) {
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
		//mainWindow->installEventFilter(new keyPressCatcher());
		view->installEventFilter(new keyPressCatcher());
		//app->setMainWidget( view );
		mainWindow->show();

		cmd_get_hook = qt_get_cmd;
	}

	void term_xtra_clear() {
		for (int r=0;r<24;r++) {
			for (int c=0;c<80;c++) {
				screen[r][c]->setPlainText(" ");
			}
		}
	}
	
	errr term_text(int x, int y, int n, byte a, const wchar_t *s) {
		char str[2];
		str[1] = '\0';
		for (int i=0; i < n; i++) {
			wchar_t c = s[i];
			str[0] = c < 256 ? c : '*';
			screen[y][x+i]->setPlainText(str);
		}
		return 0;
	}

	errr term_wipe(int x, int y, int n) {
		for (int i=0; i < n; i++) {
			screen[y][x+i]->setPlainText(" ");
		}
		return 0;
	}
	
	QQueue<int> *get_key_queue() {
		return &key_queue;
	}

private:
	QGraphicsTextItem *screen[24][80];
	QMainWindow *mainWindow;
	QGraphicsScene *scene;
	QGraphicsView *view;
	QQueue<int> key_queue;
} *app;

bool keyPressCatcher::eventFilter(QObject* object, QEvent* event) {
	std::cout << "keyPressCatcher: " << (int)event << "\n";
	if (event->type() == QEvent::KeyPress) {
		keycode_t key;
		QKeyEvent *keyEvent = dynamic_cast<QKeyEvent *>(event);

		switch(keyEvent->key()) {
		case Qt::Key_Up:     key = ARROW_UP;    break;
		case Qt::Key_Down:   key = ARROW_DOWN;  break;
		case Qt::Key_Left:   key = ARROW_LEFT;  break;
		case Qt::Key_Right:  key = ARROW_RIGHT; break;
		case Qt::Key_Enter:  key = KC_ENTER;    break;
		case Qt::Key_Return: key = KC_ENTER;    break;
		case Qt::Key_Escape: key = ESCAPE;      break;
		case Qt::Key_Tab:    key = KC_TAB;      break;
		default:
			QByteArray ba = keyEvent->text().toAscii();
			if(ba.length() != 1) {
				std::cout << "unknown key: " << keyEvent->key() << "\n";
				return true;
			}
			key = ba.at(0);
		}

		app->get_key_queue()->enqueue((int)key);
		return true;
	} else {
		// standard event processing
		return QObject::eventFilter(object, event);
	}
}

static errr term_text_qt(int x, int y, int n, byte a, const wchar_t *s) {
	//app->postEvent(app, new AngTextEvent(x,y,n,a,s));
	return app->term_text(x,y,n,a,s);
}

static errr term_wipe_qt(int x, int y, int n) {
	return app->term_wipe(x,y,n);
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

#define CHECK_EVENTS_DRAIN -1
#define CHECK_EVENTS_NO_WAIT	0
#define CHECK_EVENTS_WAIT 1

static bool check_events(int wait) {
	if (wait == CHECK_EVENTS_WAIT) {
		while (app->get_key_queue()->isEmpty()) {
			app->processEvents();
		}
	}
	if (wait != CHECK_EVENTS_DRAIN &&
		!app->get_key_queue()->isEmpty()) {
		Term_keypress(app->get_key_queue()->dequeue(), 0);
	}
	return FALSE;
}

static errr term_xtra_qt(int n, int v) {
	int res=0;
	// std::cout << "term_xtra_qt(" << n << "," << v << ")\n";
	switch(n) {
	case TERM_XTRA_EVENT:
		check_events(v);
		break;
	case TERM_XTRA_BORED:
		check_events(CHECK_EVENTS_NO_WAIT);
		break;
	case TERM_XTRA_CLEAR:
		std::cout << "TERM_XTRA_CLEAR\n";
		app->term_xtra_clear();
		break;
	case TERM_XTRA_FROSH:
	case TERM_XTRA_FRESH:
		check_events(CHECK_EVENTS_DRAIN);
		break;
	case TERM_XTRA_FLUSH: std::cout << "TERM_XTRA_FLUSH\n"; res=-1; break;
	case TERM_XTRA_SHAPE: std::cout << "TERM_XTRA_SHAPE\n"; res=-1; break;
	case TERM_XTRA_NOISE: std::cout << "TERM_XTRA_NOISE\n"; res=-1; break;
	case TERM_XTRA_REACT: std::cout << "TERM_XTRA_REACT\n"; res=-1; break;
	case TERM_XTRA_ALIVE: std::cout << "TERM_XTRA_ALIVE\n"; res=-1; break;
	case TERM_XTRA_LEVEL: std::cout << "TERM_XTRA_LEVEL\n"; res=-1; break;
	case TERM_XTRA_DELAY: std::cout << "TERM_XTRA_DELAY\n"; res=-1; break;
	}
	return res;
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
    term_init(t, 80, 24, 256 /* keypresses, for some reason? */);

    /* Prepare the init/nuke hooks */
    t->init_hook = term_init_qt;
    t->nuke_hook = term_nuke_qt;
    
    /* Prepare the function hooks */
    t->xtra_hook = term_xtra_qt;
    t->wipe_hook = term_wipe_qt;
    t->curs_hook = term_curs_qt;
    t->text_hook = term_text_qt;
    t->pict_hook = term_pict_qt;

	t->complex_input = TRUE;
    
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
	play_game();
	cleanup_angband();
}

int main( int argc, char **argv ) {
	app = new AngApp( argc, argv );

	/* Initialize some stuff */
	init_stuff();

	term_data td;
	term_data_init(&td, 0);
	Term_activate(&td.t);

	// GameThread *game_thread = new GameThread();
	// game_thread->start();

	//return app->exec();

	play_game();
	cleanup_angband();
}

#include "main-qt.moc"
