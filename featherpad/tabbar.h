/*
 * Copyright (C) Pedram Pourang (aka Tsu Jan) 2014 <tsujan2000@gmail.com>
 *
 * FeatherPad is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FeatherPad is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TABBAR_H
#define TABBAR_H

#include <QTabBar>

namespace FeatherPad {

/* The tab dropping signal (for tab detaching) should come here and not in TabWidget
   because, otherwise, the tabMoved() signal wouldn't do its job completely. */
class TabBar : public QTabBar
{
    Q_OBJECT

public:
    TabBar (QWidget *parent = 0);

signals:
    void tabDropped (QPoint&);

protected:
    /* from qtabbar.cpp */
    virtual void mouseReleaseEvent (QMouseEvent *event);
    virtual void mouseMoveEvent (QMouseEvent *event);
    bool event (QEvent *);
};

}

#endif // TABBAR_H
