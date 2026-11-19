// Генератор тестовых файлов для TypoScript.
// Создаёт UTF-8 файлы в test_samples/, по одному домену в каждом, плюс граничные случаи.
// Запуск: node build_test_samples.js (из каталога Poject/TypoScript/)

const fs = require('fs');
const path = require('path');

const ROOT = path.join(__dirname, 'test_samples');
const STRESS = path.join(ROOT, 'stress');

if (!fs.existsSync(ROOT)) fs.mkdirSync(ROOT, { recursive: true });
if (!fs.existsSync(STRESS)) fs.mkdirSync(STRESS, { recursive: true });

// Все файлы пишем в UTF-8 без BOM (программа сама определяет кодировку),
// кроме edge_with_bom.txt — для проверки обработки UTF-8 BOM.

function write(rel, content) {
  fs.writeFileSync(path.join(ROOT, rel), content, 'utf8');
  console.log('  ' + rel + ' (' + Buffer.byteLength(content, 'utf8') + ' bytes)');
}

function writeBytes(rel, bytes) {
  fs.writeFileSync(path.join(ROOT, rel), bytes);
  console.log('  ' + rel + ' (' + bytes.length + ' bytes, raw bytes)');
}

console.log('=== Категория 1: Точные совпадения с эталонными доменами ===');
write('exact_google.txt',   'google.com');
write('exact_twitter.txt',  'twitter.com');
write('exact_github.txt',   'github.com');
write('exact_yandex.txt',   'yandex.ru');
write('exact_mail.txt',     'mail.ru');
write('exact_vk.txt',       'vk.com');

console.log('');
console.log('=== Категория 2: Гомоглифические атаки (кириллица вместо латиницы) ===');
// "о" — U+043E (кириллическая), "а" — U+0430, "е" — U+0435 и т.д.
// Записываем содержимое с реальными кириллическими символами в UTF-8.
write('homoglyph_google_2cyr_o.txt', 'gооgle.com');           // две кириллические "о"
write('homoglyph_amazon_cyr_a.txt',  'аmazon.com');            // кириллическая "а" в начале
write('homoglyph_vk_cyr_o.txt',      'vk.cоm');                // кириллическая "о" в .cоm
write('homoglyph_yandex_cyr_a.txt',  'yаndex.ru');             // кириллическая "а"
write('homoglyph_twitter_cyr_o.txt', 'twitter.cоm');           // кириллическая "о"
write('homoglyph_paypal_cyr_a.txt',  'pаypal.com');            // кириллическая "а"
write('homoglyph_apple_cyr_e.txt',   'applе.com');             // кириллическая "е"

console.log('');
console.log('=== Категория 3: Однобуквенные опечатки и цифро-буквенные подмены (расстояние = 1) ===');
write('typo_gogle.txt',          'gogle.com');         // пропущенная "o" → L = 1
write('typo_gooogle.txt',        'gooogle.com');       // лишняя "o" → L = 1
write('typo_whatzapp.txt',       'whatzapp.com');      // s → z → L = 1
write('typo_netfllix.txt',       'netfllix.com');      // вставка "l" → L = 1
write('typo_teleegram.txt',      'teleegram.org');     // вставка "e" → L = 1
write('digit_paypa1.txt',        'paypa1.com');        // 1 вместо l → после нормализации L = 0
write('digit_g00gle.txt',        'g00gle.com');        // 00 вместо oo → после нормализации L = 0
write('digit_y0utube.txt',       'y0utube.com');       // 0 вместо o → после нормализации L = 0

console.log('');
console.log('=== Категория 4: Двухбуквенные модификации (расстояние Левенштейна = 2) ===');
// Все приведённые ниже домены реально имеют L = 2 относительно эталона:
// две перестановки соседних букв в Левенштейне = 2 операции замены.
write('distance2_googel.txt',     'googel.com');     // google.com: swap el↔le = 2 sub
write('distance2_amazno.txt',     'amazno.com');     // amazon.com: swap on↔no = 2 sub
write('distance2_microsfot.txt',  'microsfot.com');  // microsoft.com: swap of↔fo = 2 sub
write('distance2_yanedx.txt',     'yanedx.ru');      // yandex.ru: swap de↔ed = 2 sub
write('distance2_facbeook.txt',   'facbeook.com');   // facebook.com: swap eb↔be = 2 sub
write('distance2_micorsoft.txt',  'micorsoft.com');  // microsoft.com: swap ro↔or = 2 sub
write('distance2_twiterr.txt',    'twiterr.com');    // twitter.com: swap te↔et + r вместо t = 2 sub

console.log('');
console.log('=== Категория 5: URL и сложные форматы (проверка ExtractHost) ===');
// Этот файл соответствует кейсу из главы 5.5 ПЗ — слово в слово.
write('url_https_with_port.txt', 'https://www.google.com:443/page?q=1');
write('url_http_no_www.txt',     'http://github.com/user/repo');
write('url_only_scheme.txt',     'https://yandex.ru');
write('url_with_fragment.txt',   'https://twitter.com/user#tweets');

console.log('');
console.log('=== Граничные случаи (stress/) ===');

function writeStress(rel, content) {
  fs.writeFileSync(path.join(STRESS, rel), content, 'utf8');
  console.log('  stress/' + rel + ' (' + Buffer.byteLength(content, 'utf8') + ' bytes)');
}
function writeStressBytes(rel, bytes) {
  fs.writeFileSync(path.join(STRESS, rel), bytes);
  console.log('  stress/' + rel + ' (' + bytes.length + ' bytes, raw)');
}

// Пустой файл
writeStressBytes('edge_empty.txt', Buffer.alloc(0));

// Только UTF-8 BOM, без содержимого
writeStressBytes('edge_bom_only.txt', Buffer.from([0xEF, 0xBB, 0xBF]));

// BOM + домен с гомоглифом (проверка корректного пропуска BOM)
const bomDomain = Buffer.concat([
  Buffer.from([0xEF, 0xBB, 0xBF]),
  Buffer.from('gооgle.com', 'utf8'),
]);
writeStressBytes('edge_bom_then_homoglyph.txt', bomDomain);

// Недопустимые символы в доменном имени + гомоглиф (по сценарию главы 5.5 ПЗ)
// Здесь "а" — кириллическая (U+0430), а не латинская — это и есть гомоглиф.
// Программа должна классифицировать как подозрительный, несмотря на наличие подчёркивания.
writeStress('edge_underscore.txt',     'mа_il.ru');             // подчёркивание + кириллическая "а"
writeStress('edge_underscore_pure.txt', 'ma_il.ru');            // только подчёркивание, без гомоглифа
writeStress('edge_spaces.txt',         '  google.com  ');      // лидирующие и хвостовые пробелы
writeStress('edge_uppercase.txt',      'GOOGLE.COM');           // верхний регистр
writeStress('edge_mixed_case.txt',     'GoOgLe.CoM');           // смешанный регистр

// Длинная строка повторов (но не превышающая 256 КБ)
writeStress('edge_long_repeat.txt', 'a'.repeat(10000) + '.com');

// Неструктурированный текст без признаков домена
writeStress('edge_garbage.txt', 'это просто текстовый файл, не содержащий доменного имени');

// Только пробельные символы
writeStress('edge_whitespace_only.txt', '   \t\n   \r\n  ');

// Битый UTF-8 (некорректные последовательности байт)
writeStressBytes('edge_bad_utf8.txt', Buffer.from([
  0xFF, 0xFE, 0xFD, 0xFC,  // мусор
  0xC0, 0x80,              // overlong NUL
  0xED, 0xA0, 0x80,        // суррогатная пара (запрещена в UTF-8)
  0x67, 0x6F, 0x6F, 0x67,  // "goog"
  0x6C, 0x65, 0x2E, 0x63, 0x6F, 0x6D, // "le.com"
]));

// Нулевые байты в середине строки
writeStressBytes('edge_null_bytes.txt', Buffer.concat([
  Buffer.from('goo'),
  Buffer.from([0x00, 0x00]),
  Buffer.from('gle.com'),
]));

// Очень длинный «домен» — псевдо-стресс, ~1 МБ повторов
writeStress('edge_1mb_repeat.txt', 'x'.repeat(1024 * 1024));

console.log('');
console.log('Готово. Все файлы созданы в:');
console.log('  ' + ROOT);
console.log('  ' + STRESS);
