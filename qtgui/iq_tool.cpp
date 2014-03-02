/* -*- c++ -*- */
/*
 * Gqrx SDR: Software defined radio receiver powered by GNU Radio and Qt
 *           http://gqrx.dk/
 *
 * Copyright 2014 Alexandru Csete OZ9AEC.
 *
 * Gqrx is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * Gqrx is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Gqrx; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */
#include <QMessageBox>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QString>
#include <QStringList>
#include <QTime>

#include "iq_tool.h"
#include "ui_iq_tool.h"


CIqTool::CIqTool(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::CIqTool)
{
    ui->setupUi(this);

    is_recording = false;
    is_playing = false;
    bytes_per_sample = 8;
    sample_rate = 192000;

    ui->locationEntry->setText(QDir::currentPath());

    recdir = new QDir("", "*.raw");

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(refreshDir()));
}

CIqTool::~CIqTool()
{
    timer->stop();
    delete timer;
    delete ui;
    delete recdir;

}

/*! \brief Set new sample rate. */
void CIqTool::setSampleRate(qint64 sr)
{
    sample_rate = sr;

    if (!current_file.isEmpty())
    {
        // Get duration of selected recording and update label
        QFileInfo info(*recdir, current_file);
        rec_len = (int)(info.size() / (sample_rate * bytes_per_sample));
        refreshTimeWidgets();
    }
}

/*! \brief Slot activated when the user selects a file. */
void CIqTool::on_listWidget_currentTextChanged(const QString &currentText)
{

    current_file = currentText;
    QFileInfo info(*recdir, current_file);

    // Get duration of selected recording and update label
    sample_rate = sampleRateFromFileName(currentText);
    rec_len = (int)(info.size() / (sample_rate * bytes_per_sample));

    refreshTimeWidgets();

    // launch waveform plotter
    if (ui->plotButton->isChecked())
    {
        qDebug() << "plot plot ...";
    }
}

/*! \brief Start/stop playback */
void CIqTool::on_playButton_clicked(bool checked)
{
    is_playing = checked;

    if (checked)
    {
        if (current_file.isEmpty())
        {
            QMessageBox msg_box;
            msg_box.setIcon(QMessageBox::Critical);
            if (ui->listWidget->count() == 0)
            {
                msg_box.setText(tr("There are no I/Q files in the current directory."));
            }
            else
            {
                msg_box.setText(tr("Please select a file to play."));
            }
            msg_box.exec();

            ui->playButton->setChecked(false); // will not trig clicked()
        }
        else
        {
            ui->listWidget->setEnabled(false);
            emit startPlayback(recdir->absoluteFilePath(current_file), (float)sample_rate);
        }
    }
    else
    {
        emit stopPlayback();
        ui->listWidget->setEnabled(true);
    }
}

/*! \brief Cancel playback.
 *
 * This slot can be activated to cancel an ongoing playback.
 *
 * This slot should be used to signal that a playback could not be started.
 */
void CIqTool::cancelPlayback()
{
    ui->playButton->setChecked(false);
    ui->listWidget->setEnabled(true);
    is_playing = false;
}


/*! \brief Slider value (seek position) has changed. */
void CIqTool::on_slider_valueChanged(int value)
{
    refreshTimeWidgets();

    qint64 seek_pos = (qint64)value * (sample_rate * bytes_per_sample);
    emit seek(seek_pos);
}


/*! \brief Start/stop recording */
void CIqTool::on_recButton_clicked(bool checked)
{
    Q_UNUSED(checked);
}

/*! \brief Cancel a recording.
 *
 * This slot can be activated to cancel an ongoing recording. Cancelling an
 * ongoing recording will stop the recording and delete the recorded file, if
 * any.
 *
 * This slot should be used to signal that a recording could not be started.
 */
void CIqTool::cancelRecording()
{

}

/*! \brief Catch window close events.
 *
 * This method is called when the user closes the audio options dialog
 * window using the window close icon. We catch the event and hide the
 * dialog but keep it around for later use.
 */
void CIqTool::closeEvent(QCloseEvent *event)
{
    timer->stop();
    hide();
    event->ignore();
}

/*! \brief Catch window show events. */
void CIqTool::showEvent(QShowEvent * event)
{
    Q_UNUSED(event);
    refreshDir();
    refreshTimeWidgets();
    timer->start(5000);
}


/*! \brief Refresh list of files in current working directory. */
void CIqTool::refreshDir()
{
    int selection = ui->listWidget->currentRow();

    recdir->refresh();
    QStringList files = recdir->entryList();

    ui->listWidget->blockSignals(true);
    ui->listWidget->clear();
    ui->listWidget->insertItems(0, files);
    ui->listWidget->setCurrentRow(selection);
    ui->listWidget->blockSignals(false);
}

/*! \brief Refresh time labels and slider position
 *
 * \note Safe for recordings > 24 hours
 */
void CIqTool::refreshTimeWidgets(void)
{
    ui->slider->setMaximum(rec_len);

    // duration
    int len = rec_len;
    int lh, lm, ls;
    lh = len / 3600;
    len = len % 3600;
    lm = len / 60;
    ls = len % 60;

    // current position
    int pos = ui->slider->value();
    int ph, pm, ps;
    ph = pos / 3600;
    pos = pos % 3600;
    pm = pos / 60;
    ps = pos % 60;

    ui->timeLabel->setText(QString("%1:%2:%3 / %4:%5:%6")
                           .arg(ph, 2, 10, QChar('0'))
                           .arg(pm, 2, 10, QChar('0'))
                           .arg(ps, 2, 10, QChar('0'))
                           .arg(lh, 2, 10, QChar('0'))
                           .arg(lm, 2, 10, QChar('0'))
                           .arg(ls, 2, 10, QChar('0')));
}


/*! \brief Extract sample rate from file name */
qint64 CIqTool::sampleRateFromFileName(const QString &filename)
{
    bool ok;
    qint64 sr;

    QStringList list = filename.split('_');

    // gqrx_yymmdd_hhmmss_freq_samprate_fc.raw
    sr = list.at(4).toLongLong(&ok);

    if (ok)
        return sr;
    else
        return sample_rate;  // return current rate
}
