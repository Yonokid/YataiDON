/*class DanBox(BaseBox):
    def __init__(self, name, color: TextureIndex, songs: list[tuple[TJAParser, int, int, int]], exams: list['Exam']):
        super().__init__(name, None, None, color)
        self.songs = songs
        self.exams = exams
        self.song_text: list[tuple[OutlinedText, OutlinedText]] = []
        self.total_notes = 0
        self.yellow_box = None
        for song, genre_index, difficulty, level in self.songs:
            notes, branch_m, branch_e, branch_n = song.notes_to_position(difficulty)
            self.total_notes += sum(1 for note in notes.play_notes if note.type < 5)
            for branch in branch_m:
                self.total_notes += sum(1 for note in branch.play_notes if note.type < 5)
            for branch in branch_e:
                self.total_notes += sum(1 for note in branch.play_notes if note.type < 5)
            for branch in branch_n:
                self.total_notes += sum(1 for note in branch.play_notes if note.type < 5)

    def load_text(self):
        super().load_text()
        self.hori_name = OutlinedText(self.text_name, tex.skin_config["dan_title"].font_size, ray.WHITE)
        for song, genre, difficulty, level in self.songs:
            title = song.metadata.title.get(global_data.config["general"]["language"], song.metadata.title["en"])
            subtitle = song.metadata.subtitle.get(global_data.config["general"]["language"], "")
            title_text = OutlinedText(title, tex.skin_config["dan_title"].font_size, ray.WHITE, vertical=True)
            font_size = tex.skin_config["dan_subtitle"].font_size if len(subtitle) < 30 else tex.skin_config["dan_subtitle"].font_size - int(10 * tex.screen_scale)
            subtitle_text = OutlinedText(subtitle, font_size, ray.WHITE, vertical=True)
            self.song_text.append((title_text, subtitle_text))
        self.text_loaded = True

    def update(self, current_time: float, is_diff_select: bool):
        super().update(current_time, is_diff_select)
        is_open_prev = self.is_open
        self.is_open = self.position == BOX_CENTER
        if not is_open_prev and self.is_open:
            self.yellow_box = YellowBox(False, is_dan=True)
            self.yellow_box.create_anim()

        if self.yellow_box is not None:
            self.yellow_box.update(True)

    def _draw_exam_box(self):
        tex.draw_texture('yellow_box', 'exam_box_bottom_right')
        tex.draw_texture('yellow_box', 'exam_box_bottom_left')
        tex.draw_texture('yellow_box', 'exam_box_top_right')
        tex.draw_texture('yellow_box', 'exam_box_top_left')
        tex.draw_texture('yellow_box', 'exam_box_bottom')
        tex.draw_texture('yellow_box', 'exam_box_right')
        tex.draw_texture('yellow_box', 'exam_box_left')
        tex.draw_texture('yellow_box', 'exam_box_top')
        tex.draw_texture('yellow_box', 'exam_box_center')
        tex.draw_texture('yellow_box', 'exam_header')

        offset = tex.skin_config["exam_box_offset"].y
        for i, exam in enumerate(self.exams):
            tex.draw_texture('yellow_box', 'judge_box', y=(i*offset))
            tex.draw_texture('yellow_box', 'exam_' + exam.type, y=(i*offset))
            counter = str(exam.red)
            margin = tex.skin_config["exam_counter_margin"].x
            if exam.type == 'gauge':
                tex.draw_texture('yellow_box', 'exam_percent', y=(i*offset))
                x_offset = tex.skin_config["exam_gauge_offset"].x
            else:
                x_offset = 0
            for j in range(len(counter)):
                tex.draw_texture('yellow_box', 'judge_num', frame=int(counter[j]), x=x_offset-(len(counter) - j) * margin, y=(i*offset))

            if exam.range == 'more':
                tex.draw_texture('yellow_box', 'exam_more', x=(x_offset*-1.7), y=(i*offset))
            elif exam.range == 'less':
                tex.draw_texture('yellow_box', 'exam_less', x=(x_offset*-1.7), y=(i*offset))

    def _draw_closed(self, x: float, y: float, outer_fade_override: float):
        tex.draw_texture('box', 'folder', frame=self.texture_index, x=x, fade=outer_fade_override)
        if self.name is not None:
            self.name.draw(outline_color=ray.BLACK, x=x + tex.skin_config["song_box_name"].x - int(self.name.texture.width / 2), y=y+(tex.skin_config["song_box_name"].height//2), y2=min(self.name.texture.height, tex.skin_config["song_box_name"].height)-self.name.texture.height, fade=outer_fade_override)

    def _draw_open(self, x: float, y: float, fade_override: Optional[float], is_ura: bool):
        if fade_override is not None:
            fade = fade_override
        else:
            fade = 1.0
        if self.yellow_box is not None:
            self.yellow_box.draw(None, None, False, self.name)
            for i, song in enumerate(self.song_text):
                title, subtitle = song
                x = i * tex.skin_config["dan_yellow_box_offset"].x
                tex.draw_texture('yellow_box', 'genre_banner', x=x, frame=self.songs[i][1], fade=fade)
                tex.draw_texture('yellow_box', 'difficulty', x=x, frame=self.songs[i][2], fade=fade)
                tex.draw_texture('yellow_box', 'difficulty_x', x=x, fade=fade)
                tex.draw_texture('yellow_box', 'difficulty_star', x=x, fade=fade)
                level = self.songs[i][0].metadata.course_data[self.songs[i][2]].level
                counter = str(level)
                margin = tex.skin_config["dan_level_counter_margin"].x
                total_width = len(counter) * margin
                for i in range(len(counter)):
                    tex.draw_texture('yellow_box', 'difficulty_num', frame=int(counter[i]), x=x-(total_width // 2) + (i * margin), fade=fade)

                title_data = tex.skin_config["dan_title"]
                subtitle_data = tex.skin_config["dan_subtitle"]
                title.draw(outline_color=ray.BLACK, x=title_data.x+x, y=title_data.y, y2=min(title.texture.height, title_data.height)-title.texture.height, fade=fade)
                subtitle.draw(outline_color=ray.BLACK, x=subtitle_data.x+x, y=subtitle_data.y-min(subtitle.texture.height, subtitle_data.height), y2=min(subtitle.texture.height, subtitle_data.height)-subtitle.texture.height, fade=fade)

            tex.draw_texture('yellow_box', 'total_notes_bg', fade=fade)
            tex.draw_texture('yellow_box', 'total_notes', fade=fade)
            counter = str(self.total_notes)
            for i in range(len(counter)):
                tex.draw_texture('yellow_box', 'total_notes_counter', frame=int(counter[i]), x=(i * tex.skin_config["total_notes_counter_margin"].x), fade=fade)

            tex.draw_texture('yellow_box', 'frame', frame=self.texture_index, fade=fade)
            if self.hori_name is not None:
                self.hori_name.draw(outline_color=ray.BLACK, x=tex.skin_config["dan_hori_name"].x - (self.hori_name.texture.width//2), y=tex.skin_config["dan_hori_name"].y, x2=min(self.hori_name.texture.width, tex.skin_config["dan_hori_name"].width)-self.hori_name.texture.width, fade=fade)

                self._draw_exam_box()*/
