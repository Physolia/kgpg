#ifndef KGPGADDPHOTO_TEST_H
#define KGPGADDPHOTO_TEST_H

#include <QObject>
#include <QTemporaryDir>

class KGpgAddPhotoTest : public QObject {
	Q_OBJECT
private slots:
	void init();
	void testAddPhoto();

private:
	QTemporaryDir m_tempdir;
};

#endif
