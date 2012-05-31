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

static errr get_cmd_init();

static errr qt_get_cmd(cmd_context context, bool wait)
{
	// std::cout << "qt_get_cmd()\n";
	if (context == CMD_INIT) 
		return get_cmd_init();
	else 
		return textui_get_cmd(context, wait);
}

void msleep(unsigned long msec) {
	QMutex m;
	m.lock();
	QWaitCondition w;
	w.wait(&m, msec);
	m.unlock();
}

class keyPressCatcher : public QObject {
public:
	keyPressCatcher() {}
	~keyPressCatcher() {}
	bool eventFilter(QObject* object, QEvent* event);
};

class AngApp : public QApplication {

	Q_OBJECT

public:
	AngApp( int argc, char **argv ) : QApplication(argc, argv) {
		mainWindow = new QMainWindow();
		scene = new QGraphicsScene(QRectF(0, 0, 80*8, 24*10));
		scene->setBackgroundBrush(Qt::black);
		view = new QGraphicsView(scene);
		QFont myfont("Courier", 10);
		QFontInfo myfontinfo(myfont);

		colortable[TERM_DARK]    = QColor( 0*8,  0*8,  0*8);
		colortable[TERM_WHITE]   = QColor(31*8, 31*8, 31*8);
		colortable[TERM_SLATE]   = QColor(15*8, 15*8, 15*8);
		colortable[TERM_ORANGE]  = QColor(31*8, 15*8,  0*8);
		colortable[TERM_RED]     = QColor(23*8,  0*8,  0*8);
		colortable[TERM_GREEN]   = QColor( 0*8, 15*8,  9*8);
		colortable[TERM_BLUE]    = QColor( 0*8,  0*8, 31*8);
		colortable[TERM_UMBER]   = QColor(15*8,  9*8,  0*8);
		colortable[TERM_L_DARK]  = QColor( 9*8,  9*8,  9*8);
		colortable[TERM_L_WHITE] = QColor(23*8, 23*8, 23*8);
		colortable[TERM_VIOLET]  = QColor(31*8,  0*8, 31*8);
		colortable[TERM_YELLOW]  = QColor(31*8, 31*8,  0*8);
		colortable[TERM_L_RED]   = QColor(31*8,  0*8,  0*8);
		colortable[TERM_L_GREEN] = QColor( 0*8, 31*8,  0*8);
		colortable[TERM_L_BLUE]  = QColor( 0*8, 31*8, 31*8);
		colortable[TERM_L_UMBER] = QColor(23*8, 15*8,  9*8);

		for (int r=0;r<24;r++) {
			for (int c=0;c<80;c++) {
				QGraphicsTextItem *text = scene->addText(" ");
				text->setFont(myfont);
				text->document()->setDocumentMargin(0);
				text->setDefaultTextColor(Qt::white);
				if (!r && !c) {
					// figure out font size from the size of the first char
					QRectF rect = text->boundingRect();
					char_height = text->document()->size().height();
					char_width = text->document()->size().width();
					scene->setSceneRect(0,0,char_width*80,char_height*24);
					cursor = scene->addRect(0, 0, char_width, char_height,
											Qt::SolidLine, Qt::green);
					// std::cout << "width = " << width << "\n";
				}
				text->translate(c*char_width,r*char_height);
				screen[r][c] = text;
			}
		}

		setQuitOnLastWindowClosed(TRUE);

		mainWindow->setCentralWidget(view);
		QMenuBar *mb = mainWindow->menuBar();
		QMenu *file_menu = mb->addMenu(tr("&File"));

		QAction *new_action = new QAction(tr("&New"), this);
		connect(new_action, SIGNAL(triggered()), this, SLOT(newGame()));
		file_menu->addAction(new_action);

		QAction *open_action = new QAction(tr("&Open"), this);
		connect(open_action, SIGNAL(triggered()), this, SLOT(openFile()));
		file_menu->addAction(open_action);

		exit_action = new QAction(tr("E&xit"), this);
		connect(exit_action, SIGNAL(triggered()), this,
				SLOT(closeAllWindows()));
		file_menu->addAction(exit_action);
		
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
		int attr = a & 127;
		// std::cout << "color = " << attr << "\n";
		for (int i=0; i < n; i++) {
			wchar_t c = s[i];
			str[0] = c < 256 ? c : '*';
			screen[y][x+i]->setPlainText(str);
			screen[y][x+i]->setDefaultTextColor(colortable[attr]);
		}
		moveCursor(x,y);
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

	void moveCursor(int x, int y) {
		cursor->setPos(char_width*x,char_height*y);
	}

	void cursSet(int v) {
		cursor->setBrush(v ? Qt::green : Qt::black);
	}

public slots:
	void openFile() {
		QString fileName =
			QFileDialog::getOpenFileName(mainWindow, tr("Open Character"),
										 ANGBAND_DIR_SAVE,
										 tr("All Files (*.*)"));
		strcpy(savefile, fileName.toAscii().data());
		cmd.command = CMD_LOADFILE;
	}

	void newGame() {
		cmd.command = CMD_NEWGAME;
	}

private:
	QGraphicsTextItem *screen[24][80];
	QGraphicsRectItem *cursor;
	QMainWindow *mainWindow;
	QGraphicsScene *scene;
	QGraphicsView *view;
	QQueue<int> key_queue;
	QColor colortable[BASIC_COLORS];
	QAction *exit_action;
	qreal char_height;
	qreal char_width;
} *app;

bool keyPressCatcher::eventFilter(QObject* object, QEvent* event) {
	//std::cout << "keyPressCatcher: " << (int)event << "\n";
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
				// std::cout << "unknown key: " << keyEvent->key() << "\n";
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
	// std::cout << "term_init_qt()\n";
}

static void term_nuke_qt(term *t) {
	// std::cout << "term_nuke_qt()\n";
}

#define CHECK_EVENTS_DRAIN -1
#define CHECK_EVENTS_NO_WAIT	0
#define CHECK_EVENTS_WAIT 1

static bool check_events(int wait) {
	// TODO: we need to handle DeferredEvents here.
	if (wait == CHECK_EVENTS_WAIT) {
		while (app->get_key_queue()->isEmpty()) {
			app->processEvents(QEventLoop::AllEvents, 100);
			msleep(100);
		}
	}
	if (wait != CHECK_EVENTS_DRAIN &&
		!app->get_key_queue()->isEmpty()) {
		Term_keypress(app->get_key_queue()->dequeue(), 0);
	}
	return FALSE;
}

static errr get_cmd_init() {
	// std::cout << "get_cmd_init()\n";
    if (cmd.command == CMD_NULL)
    {
        /* Prompt the user */ 
        prt("[Choose 'New' or 'Open' from the 'File' menu]", 23, 17);
        Term_fresh();
        
        while (cmd.command == CMD_NULL) {
			app->processEvents();
			msleep(100);
		}
    }
    
    cmd_insert_s(&cmd);
	return 0;
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
		// std::cout << "TERM_XTRA_CLEAR\n";
		app->term_xtra_clear();
		break;
	case TERM_XTRA_FROSH:
	case TERM_XTRA_FRESH:
		check_events(CHECK_EVENTS_DRAIN);
		break;
	case TERM_XTRA_SHAPE:
		app->cursSet(v);
		break;
	// case TERM_XTRA_FLUSH: std::cout << "TERM_XTRA_FLUSH\n"; res=-1; break;
	// case TERM_XTRA_NOISE: std::cout << "TERM_XTRA_NOISE\n"; res=-1; break;
	// case TERM_XTRA_REACT: std::cout << "TERM_XTRA_REACT\n"; res=-1; break;
	// case TERM_XTRA_ALIVE: std::cout << "TERM_XTRA_ALIVE\n"; res=-1; break;
	// case TERM_XTRA_LEVEL: std::cout << "TERM_XTRA_LEVEL\n"; res=-1; break;
	// case TERM_XTRA_DELAY: std::cout << "TERM_XTRA_DELAY\n"; res=-1; break;
	default:
		res=-1;
		break;
	}
	return res;
}

static errr term_curs_qt(int x, int y) {
	// std::cout << "term_curs_qt(" << x << "," << y << ")\n";
	app->moveCursor(x,y);
	return 0;
}

static errr term_pict_qt(int x, int y, int n, const byte *ap,
						 const wchar_t *cp, const byte *tap,
						 const wchar_t *tcp) {
	// std::cout << "term_pict_qt()\n";
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
	// std::cout << "starting game thread\n";
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
