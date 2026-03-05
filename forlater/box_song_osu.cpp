/*class SongBoxOsu(SongBox):
    def update(self, current_time: float, is_diff_select: bool):
        super().update(current_time, is_diff_select)
        is_open_prev = self.is_open
        self.is_open = self.position == BOX_CENTER

        if self.yellow_box is not None:
            self.yellow_box.update(is_diff_select)

        if self.history_wait == 0:
            self.history_wait = current_time

        if self.score_history is None and {k: v for k, v in self.scores.items() if v is not None}:
            self.score_history = ScoreHistory(self.scores, current_time)

        if not is_open_prev and self.is_open:
            self.yellow_box = YellowBox(False)
            self.yellow_box.create_anim()
            self.wait = current_time
            if current_time >= self.history_wait + 3000:
                self.history_wait = current_time

        if self.score_history is not None:
            self.score_history.update(current_time)

    def _draw_closed(self, x: float, y: float, outer_fade_override: float):
        super()._draw_closed(x, y, outer_fade_override)

        self.name.draw(outline_color=self.fore_color, x=x + tex.skin_config["song_box_name"].x - int(self.name.texture.width / 2), y=y+tex.skin_config["song_box_name"].y, y2=min(self.name.texture.height, tex.skin_config["song_box_name"].height)-self.name.texture.height, fade=outer_fade_override)
        valid_scores = {k: v for k, v in self.scores.items() if v is not None}
        if valid_scores:
            highest_key = max(valid_scores.keys())
            score = self.scores[highest_key]
            if score and score[5] == Crown.DFC:
                tex.draw_texture('yellow_box', 'crown_dfc', x=x, y=y, frame=min(Difficulty.URA, highest_key), fade=outer_fade_override)
            elif score and score[5] == Crown.FC:
                tex.draw_texture('yellow_box', 'crown_fc', x=x, y=y, frame=min(Difficulty.URA, highest_key), fade=outer_fade_override)
            elif score and score[5] >= Crown.CLEAR:
            tex.draw_texture('yellow_box', 'crown_clear', x=x, y=y, frame=min(Difficulty.URA, highest_key), fade=outer_fade_override)*/
