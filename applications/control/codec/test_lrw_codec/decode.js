/*

See README.md for usage instructions

*/

const fs = require('fs');

// Include the decoder module
const decoder = require('../cs-decoder.js');

// Named arguments parser
const args = process.argv.slice(2);
function getArg(name, defaultValue) {
    const index = args.indexOf(`--${name}`);
    if (index !== -1 && index + 1 < args.length) {
        return args[index + 1];
    }
    return defaultValue;
}

if (getArg('folder', null)) {
    const folder = getArg('folder', 'data/');
    const files = fs.readdirSync(folder);

    files.forEach(file => {
        if (file.endsWith('.json')) {
            const base64File = `${folder}/${file.replace('.json', '.b64')}`;
            const inputJsonFile = `${folder}/${file}`;

            inputJson = JSON.parse(fs.readFileSync(inputJsonFile, 'utf8'));
            base64 = fs.readFileSync(base64File, 'utf8').trim();

            var hex = Buffer.from(base64, "base64").toString("hex");
            var buf = Buffer.from(hex, "hex");
            decode = decoder.decodeUplink({ bytes: buf });

            console.log(`Comparing files: ${base64File} and ${inputJsonFile}`);
            printDecode(decode);
            compare(inputJson, decode);
        }
    });
} else if (getArg('b64', null) && getArg('json', null)) {
    const base64File = getArg('b64', "");
    const inputJsonFile = getArg('json', "");

    inputJson = JSON.parse(fs.readFileSync(inputJsonFile, 'utf8'));
    base64 = fs.readFileSync(base64File, 'utf8').trim();

    var hex = Buffer.from(base64, "base64").toString("hex");
    var buf = Buffer.from(hex, "hex");
    decode = decoder.decodeUplink({ bytes: buf });

    printDecode(decode);
    compare(inputJson, decode);

} else if (getArg('b64str', null)) {
    base64 = getArg('b64str', "");

    var hex = Buffer.from(base64, "base64").toString("hex");
    var buf = Buffer.from(hex, "hex");
    decode = decoder.decodeUplink({ bytes: buf });

    printDecode(decode);
    compare(inputJson, decode);

} else {
    console.log("wrong parameters");
}

function printDecode(decode) {
    if (getArg('print', null)) {
        console.log(JSON.stringify(decode, null, 2));
    }
}


function compare(inputJson, decode) {
    if (getArg('compare', null) === 'y') {
        const decodeStr = JSON.stringify(decode, null, 2);
        const inputStr = JSON.stringify(inputJson, null, 2);

        if (decodeStr === inputStr) {
            console.log('\x1b[32mComparison result: MATCH\x1b[0m');
        } else {
            console.log('\x1b[31mComparison result: DIFFER\x1b[0m');
            const decodeLines = decodeStr.split('\n');
            const inputLines = inputStr.split('\n');
            const maxLines = Math.max(decodeLines.length, inputLines.length);

            for (let i = 0; i < maxLines; i++) {
                const dLine = decodeLines[i] || '';
                const iLine = inputLines[i] || '';
                if (dLine !== iLine) {
                    console.log(`Difference at line ${i + 1}:`);
                    console.log(`input  : ${iLine}`);
                    console.log(`decode : ${dLine}`);
                    break;
                }
            }
            process.exit(1);
        }
    }
}
