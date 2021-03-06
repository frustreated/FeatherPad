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

#ifndef CONFIG_H
#define CONFIG_H

#include <QSize>
#include <QFont>

namespace FeatherPad {

class Config {
public:
    Config();
    ~Config();

    void readConfig();
    void writeConfig();

    bool getRemSize() const {
        return remSize_;
    }
    void setRemSize (bool rem) {
        remSize_ = rem;
    }

    bool getIsMaxed() const {
        return isMaxed_;
    }
    void setIsMaxed (bool isMaxed) {
        isMaxed_ = isMaxed;
    }

    bool getIsFull() const {
        return isFull_;
    }
    void setIsFull (bool isFull) {
        isFull_ = isFull;
    }

    QSize getWinSize() const {
        return winSize_;
    }
    void setWinSize (QSize s) {
        winSize_ = s;
    }

    bool getNoToolbar() const {
        return noToolbar_;
    }
    void setNoToolbar (bool noTB) {
        noToolbar_ = noTB;
    }

    bool getHideSearchbar() const {
        return hideSearchbar_;
    }
    void setHideSearchbar (bool hide) {
        hideSearchbar_ = hide;
    }

    bool getShowStatusbar() const {
        return showStatusbar_;
    }
    void setShowStatusbar (bool show) {
        showStatusbar_ = show;
    }

    QFont getFont() const {
        return font_;
    }
    void setFont (QFont font) {
        font_ = font;
    }

    bool getRemFont() const {
        return remFont_;
    }
    void setRemFont (bool rem) {
        remFont_ = rem;
    }

    bool getWrapByDefault() const {
        return wrapByDefault_;
    }
    void setWrapByDefault (bool wrap) {
        wrapByDefault_ = wrap;
    }

    bool getIndentByDefault() const {
        return indentByDefault_;
    }
    void setIndentByDefault (bool indent) {
        indentByDefault_ = indent;
    }


    bool getLineByDefault() const {
        return lineByDefault_;
    }
    void setLineByDefault (bool line) {
        lineByDefault_ = line;
    }

    bool getSyntaxByDefault() const {
        return syntaxByDefault_;
    }
    void setSyntaxByDefault (bool syntax) {
        syntaxByDefault_ = syntax;
    }

    int getMaxSHSize() const {
        return maxSHSize_;
    }
    void setMaxSHSize (int max) {
        maxSHSize_ = max;
    }

    bool getScrollJumpWorkaround() const {
        return scrollJumpWorkaround_;
    }
    void setScrollJumpWorkaround (bool workaround) {
        scrollJumpWorkaround_ = workaround;
    }

private:
    bool remSize_, noToolbar_, hideSearchbar_, showStatusbar_, remFont_, wrapByDefault_,
         indentByDefault_, lineByDefault_, syntaxByDefault_, isMaxed_, isFull_,
         // Should a workaround for Qt5's "scroll jump" bug be applied?
         scrollJumpWorkaround_;
    int maxSHSize_;
    QSize winSize_;
    QFont font_;
};

}

#endif // CONFIG_H
