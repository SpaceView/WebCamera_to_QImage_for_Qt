#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "WebCamera.h"

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>

QGraphicsView* gView = NULL;
QGraphicsScene* gScene = NULL;
WebCamera *gCam = NULL;
QGraphicsPixmapItem* gItem = NULL;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(ui->btnOpenCamera, &QPushButton::clicked, this, &MainWindow::onOpenCamera);
    connect(ui->btnCloseCamera, &QPushButton::clicked, this, &MainWindow::onCloseCamera);
    connect(ui->btnGrabImage, &QPushButton::clicked, this, &MainWindow::onGrabImage);

    gCam = new WebCamera();
    connect(gCam, &WebCamera::onImageOutput, this, &MainWindow::onImageReceive);

    gScene = new QGraphicsScene(0, 0, 800, 600);
    gView = new QGraphicsView;
    gView->setScene(gScene);
    ui->gridLayout_cam->addWidget(gView); //setCentralWidget(view);

    gItem = new QGraphicsPixmapItem;
    gScene->addItem(gItem);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onImageReceive(QImage& image)
{
    if(gScene){
        gScene->setSceneRect(image.rect());
    }
    if(gItem){
        gItem->setPixmap(QPixmap::fromImage(image));
    }
}

void MainWindow::onOpenCamera()
{
    gCam->OpenCamera();
}

void MainWindow::onCloseCamera()
{
    gCam->CloseCamera();
}

void MainWindow::onGrabImage()
{

}


